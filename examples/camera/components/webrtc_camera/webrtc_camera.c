/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "webrtc_camera.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_peer.h"
#include "esp_peer_default.h"
#include "esp_wifi.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "camera_controls.h"
#include "jpeg_streamer.h"
#include "webrtc_camera_priv.h"

static const char *TAG = "webrtc_camera";

#define MAX_SIGNAL_BYTES     (16 * 1024)
#define MAX_ICE_SERVERS      8
#define MAX_CANDIDATES       16
/* esp_peer reports its SDP after gathering; stray CANDIDATE messages may trail it briefly. */
#define CANDIDATE_SETTLE_MS  200
/* Extra wait beyond answer_timeout_ms, so the session task reports its own timeout first. */
#define ANSWER_WAIT_SLACK_MS 500
#define MAX_CONTROL_BYTES    512
/* Measured on an AI-Thinker ESP32-CAM: at -85 dBm the Wi-Fi TX buffers never recycle fast
 * enough and the DTLS handshake cannot complete. -75 leaves margin before that cliff. */
#define WEAK_SIGNAL_DBM      (-75)
#define AUDIO_FRAME_BYTES    160 /* 20 ms of 8 kHz PCMU */
#define AUDIO_FRAME_MS       20

typedef struct {
    char *url;
    char *username;
    char *credential;
} ice_server_t;

typedef enum {
    COMMAND_START,
    COMMAND_STOP,
} command_type_t;

typedef struct {
    command_type_t type;
    uint32_t sequence;
    char *offer;           /* owned; freed by the session task */
    ice_server_t *servers; /* owned; freed by the session task */
    size_t server_count;
} command_t;

struct webrtc_camera {
    webrtc_camera_config_t config;
    jpeg_streamer_t streamer;
    camera_controls_t controls;
    QueueHandle_t commands;
    SemaphoreHandle_t answer_ready;
    SemaphoreHandle_t answer_lock;
    SemaphoreHandle_t offer_lock;
    TaskHandle_t task;

    /* Written only by webrtc_camera_handle_offer(), under offer_lock. */
    uint32_t sequence;

    /* Owned by the session task. */
    esp_peer_handle_t peer;
    esp_peer_default_cfg_t peer_defaults;
    ice_server_t *servers;
    size_t server_count;
    esp_peer_ice_server_cfg_t server_cfg[MAX_ICE_SERVERS];
    char *local_sdp;
    char *candidates[MAX_CANDIDATES];
    size_t candidate_count;
    uint32_t active_sequence;
    uint32_t session_started_ms;
    uint32_t last_signal_ms;
    uint32_t audio_pts;
    bool answer_published;
    bool close_requested;
    bool audio_active;
    bool capabilities_pending;
    bool state_pending;
    uint16_t channel_id;
    atomic_bool channel_open;

    /* Handed from the session task to webrtc_camera_handle_offer(); guarded by answer_lock. */
    uint32_t answer_sequence;
    bool answer_ok;
    char *answer;
    char answer_error[128];
};

static void set_error(char *error, size_t size, const char *text)
{
    if (error != NULL && size > 0) {
        snprintf(error, size, "%s", text);
    }
}

/* The viewer sees this text, and a weak link is the usual cause of a failed handshake: below
 * about -80 dBm the Wi-Fi driver runs out of TX buffers. Naming the signal turns an opaque
 * timeout into something the user can act on. */
static void append_weak_signal(char *error, size_t size)
{
    wifi_ap_record_t ap;
    if (error == NULL || size == 0 || esp_wifi_sta_get_ap_info(&ap) != ESP_OK || ap.rssi >= WEAK_SIGNAL_DBM) {
        return;
    }
    size_t used = strlen(error);
    if (used + 1 < size) {
        snprintf(error + used, size - used, " (Wi-Fi signal %d dBm is too weak)", ap.rssi);
    }
}

static void free_servers(ice_server_t *servers, size_t count)
{
    for (size_t i = 0; servers != NULL && i < count; i++) {
        free(servers[i].url);
        free(servers[i].username);
        free(servers[i].credential);
    }
    free(servers);
}

static ice_server_t *copy_servers(const webrtc_camera_ice_server_t *source, size_t count)
{
    ice_server_t *servers = calloc(count, sizeof(*servers));
    if (servers == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        servers[i].url = strdup(source[i].url != NULL ? source[i].url : "");
        servers[i].username = strdup(source[i].username != NULL ? source[i].username : "");
        servers[i].credential = strdup(source[i].credential != NULL ? source[i].credential : "");
        if (servers[i].url == NULL || servers[i].username == NULL || servers[i].credential == NULL) {
            free_servers(servers, i + 1);
            return NULL;
        }
    }
    return servers;
}

static void free_command(command_t *command)
{
    free(command->offer);
    free_servers(command->servers, command->server_count);
    command->offer = NULL;
    command->servers = NULL;
    command->server_count = 0;
}

static void clear_signals(struct webrtc_camera *session)
{
    free(session->local_sdp);
    session->local_sdp = NULL;
    for (size_t i = 0; i < session->candidate_count; i++) {
        free(session->candidates[i]);
    }
    session->candidate_count = 0;
}

/* Signaling is a single exchange, so candidates reported separately from the SDP must ride inside
 * it. With BUNDLE every media section shares one transport, so the first section carries them. */
static char *build_answer(const struct webrtc_camera *session)
{
    const char *sdp = session->local_sdp;
    const char *eol = strstr(sdp, "\r\n") != NULL ? "\r\n" : "\n";
    size_t eol_len = strlen(eol);
    size_t sdp_len = strlen(sdp);

    size_t extra_len = 0;
    for (size_t i = 0; i < session->candidate_count; i++) {
        if (strstr(sdp, session->candidates[i]) == NULL) {
            extra_len += 2 + strlen(session->candidates[i]) + eol_len;
        }
    }
    if (extra_len == 0) {
        return strdup(sdp);
    }

    /* "\r\nm=" plus its terminator: five bytes, which CRLF SDPs from browsers need. */
    char media_line[5];
    snprintf(media_line, sizeof(media_line), "%sm=", eol);
    /* The first line is always "v=0", so every media line follows an end of line. */
    const char *first = strstr(sdp, media_line);
    const char *second = first != NULL ? strstr(first + eol_len, media_line) : NULL;

    size_t split = sdp_len;
    bool terminate = false;
    if (second != NULL) {
        split = (size_t)(second - sdp) + eol_len;
    } else {
        terminate = sdp_len < eol_len || strcmp(sdp + sdp_len - eol_len, eol) != 0;
    }

    char *out = malloc(sdp_len + extra_len + (terminate ? eol_len : 0) + 1);
    if (out == NULL) {
        return NULL;
    }

    char *cursor = out;
    memcpy(cursor, sdp, split);
    cursor += split;
    if (terminate) {
        memcpy(cursor, eol, eol_len);
        cursor += eol_len;
    }
    for (size_t i = 0; i < session->candidate_count; i++) {
        const char *candidate = session->candidates[i];
        if (strstr(sdp, candidate) != NULL) {
            continue;
        }
        size_t length = strlen(candidate);
        memcpy(cursor, "a=", 2);
        memcpy(cursor + 2, candidate, length);
        memcpy(cursor + 2 + length, eol, eol_len);
        cursor += 2 + length + eol_len;
    }
    memcpy(cursor, sdp + split, sdp_len - split);
    cursor += sdp_len - split;
    *cursor = '\0';
    return out;
}

static void publish_answer(struct webrtc_camera *session, bool ok, const char *error)
{
    char *answer = ok ? build_answer(session) : NULL;
    if (ok && answer == NULL) {
        ok = false;
        error = "Camera is out of memory";
    }

    xSemaphoreTake(session->answer_lock, portMAX_DELAY);
    /* An answer nobody collected belongs to an offer whose caller already timed out. */
    free(session->answer);
    session->answer = answer;
    session->answer_sequence = session->active_sequence;
    session->answer_ok = ok;
    snprintf(session->answer_error, sizeof(session->answer_error), "%s", error != NULL ? error : "");
    xSemaphoreGive(session->answer_lock);

    session->answer_published = true;
    xSemaphoreGive(session->answer_ready);
}

static void close_peer(struct webrtc_camera *session)
{
    jpeg_streamer_reset(&session->streamer);
    camera_controls_viewer_left(&session->controls);
    atomic_store(&session->channel_open, false);
    session->close_requested = false;
    session->audio_active = false;
    session->capabilities_pending = false;
    session->state_pending = false;

    if (session->peer != NULL) {
        esp_peer_close(session->peer);
        session->peer = NULL;
    }
    /* Freed only after the peer is closed: it keeps pointers to these strings. */
    free_servers(session->servers, session->server_count);
    session->servers = NULL;
    session->server_count = 0;

    if (!session->answer_published) {
        publish_answer(session, false, "Camera closed the WebRTC session before answering");
    }
}

static int on_msg(esp_peer_msg_t *msg, void *ctx)
{
    struct webrtc_camera *session = ctx;
    if (msg == NULL || msg->data == NULL || msg->size <= 0 || msg->size > MAX_SIGNAL_BYTES) {
        return ESP_PEER_ERR_INVALID_ARG;
    }

    if (msg->type == ESP_PEER_MSG_TYPE_SDP) {
        char *sdp = strndup((const char *)msg->data, (size_t)msg->size);
        if (sdp != NULL) {
            free(session->local_sdp);
            session->local_sdp = sdp;
        }
    } else if (msg->type == ESP_PEER_MSG_TYPE_CANDIDATE && session->candidate_count < MAX_CANDIDATES) {
        const char *text = (const char *)msg->data;
        size_t length = (size_t)msg->size;
        while (length > 0 && isspace((unsigned char)text[0])) {
            text++;
            length--;
        }
        while (length > 0 && isspace((unsigned char)text[length - 1])) {
            length--;
        }
        if (length >= 2 && strncmp(text, "a=", 2) == 0) {
            text += 2;
            length -= 2;
        }
        if (length >= 10 && strncmp(text, "candidate:", 10) == 0) {
            char *candidate = strndup(text, length);
            if (candidate != NULL) {
                session->candidates[session->candidate_count++] = candidate;
            }
        }
    }

    session->last_signal_ms = webrtc_camera_now_ms();
    return ESP_PEER_ERR_NONE;
}

static int on_state(esp_peer_state_t state, void *ctx)
{
    struct webrtc_camera *session = ctx;
    ESP_LOGI(TAG, "Peer state: %d", (int)state);
    if (state == ESP_PEER_STATE_DISCONNECTED || state == ESP_PEER_STATE_CONNECT_FAILED) {
        session->close_requested = true;
    }
    return ESP_PEER_ERR_NONE;
}

static int on_channel_open(esp_peer_data_channel_info_t *channel, void *ctx)
{
    struct webrtc_camera *session = ctx;
    session->channel_id = channel->stream_id;
    session->capabilities_pending = true;
    session->state_pending = true;
    atomic_store(&session->channel_open, true);
    return ESP_PEER_ERR_NONE;
}

static int on_channel_close(esp_peer_data_channel_info_t *channel, void *ctx)
{
    (void)channel;
    struct webrtc_camera *session = ctx;
    atomic_store(&session->channel_open, false);
    session->close_requested = true;
    return ESP_PEER_ERR_NONE;
}

static int on_data(esp_peer_data_frame_t *frame, void *ctx)
{
    struct webrtc_camera *session = ctx;
    if (frame == NULL || frame->type != ESP_PEER_DATA_CHANNEL_STRING || frame->size <= 0 ||
        frame->size > MAX_CONTROL_BYTES) {
        return ESP_PEER_ERR_NONE;
    }
    camera_controls_apply(&session->controls, (const char *)frame->data, (size_t)frame->size);
    session->state_pending = true;
    return ESP_PEER_ERR_NONE;
}

static void start_peer(struct webrtc_camera *session, command_t *command)
{
    close_peer(session);

    session->active_sequence = command->sequence;
    session->answer_published = false;
    session->session_started_ms = webrtc_camera_now_ms();
    session->last_signal_ms = session->session_started_ms;
    clear_signals(session);

    /* esp_peer keeps pointers to these strings until it is closed, so the session takes them over. */
    session->servers = command->servers;
    session->server_count = command->server_count;
    command->servers = NULL;
    command->server_count = 0;
    for (size_t i = 0; i < session->server_count; i++) {
        session->server_cfg[i] = (esp_peer_ice_server_cfg_t){
            .stun_url = session->servers[i].url,
            .user = session->servers[i].username[0] != '\0' ? session->servers[i].username : NULL,
            .psw = session->servers[i].credential[0] != '\0' ? session->servers[i].credential : NULL,
        };
    }

    esp_peer_cfg_t cfg = {
        .server_lists = session->server_count > 0 ? session->server_cfg : NULL,
        .server_num = (uint8_t)session->server_count,
        .role = ESP_PEER_ROLE_CONTROLLED,
        .enable_data_channel = true,
        .manual_ch_create = true, /* the viewer creates the channel */
        .no_auto_reconnect = true,
        .ctx = session,
        .on_msg = on_msg,
        .on_state = on_state,
        .on_channel_open = on_channel_open,
        .on_channel_close = on_channel_close,
        .on_data = on_data,
    };

    /* Viewers offer audio only when getCameraCapabilities reported it; older viewers never do. */
    session->audio_active = session->config.audio_source != NULL && strstr(command->offer, "m=audio") != NULL;
    if (session->audio_active) {
        cfg.audio_info = (esp_peer_audio_stream_info_t){ESP_PEER_AUDIO_CODEC_G711U, 8000, 1};
        cfg.audio_dir = ESP_PEER_MEDIA_DIR_SEND_ONLY;
    }

    memset(&session->peer_defaults, 0, sizeof(session->peer_defaults));
    session->peer_defaults.agent_recv_timeout = 10;
    session->peer_defaults.data_ch_cfg.send_cache_size = session->config.data_channel_send_cache;
    session->peer_defaults.data_ch_cfg.recv_cache_size = session->config.data_channel_recv_cache;
    /* RTP carries only the optional PCMU track, so a DataChannel-only session would otherwise
     * strand memory the Wi-Fi driver needs. Zero is not an option: esp_peer reads it as its
     * 400 kB default. */
    session->peer_defaults.rtp_cfg.send_pool_size = session->audio_active ? 48 * 1024 : 4 * 1024;
    session->peer_defaults.rtp_cfg.send_queue_num = session->audio_active ? 64 : 8;
    cfg.extra_cfg = &session->peer_defaults;
    cfg.extra_size = sizeof(session->peer_defaults);

    /* Callbacks fire only inside esp_peer_main_loop(), so the first SDP reported after the offer
     * is this session's answer. */
    int ret = esp_peer_open(&cfg, esp_peer_get_default_impl(), &session->peer);
    if (ret == ESP_PEER_ERR_NONE) {
        ret = esp_peer_new_connection(session->peer);
    }
    if (ret == ESP_PEER_ERR_NONE) {
        esp_peer_msg_t offer = {
            .type = ESP_PEER_MSG_TYPE_SDP,
            .data = (uint8_t *)command->offer,
            .size = (int)strlen(command->offer),
        };
        ret = esp_peer_send_msg(session->peer, &offer);
    }

    if (ret != ESP_PEER_ERR_NONE) {
        ESP_LOGE(TAG, "Peer start failed: %d", ret);
        char reason[64];
        snprintf(reason, sizeof(reason), "Camera could not start WebRTC (error %d)", ret);
        publish_answer(session, false, reason);
        close_peer(session);
    }
}

/* Takes ownership of json. */
static bool send_text(struct webrtc_camera *session, char *json)
{
    if (json == NULL) {
        return false;
    }
    esp_peer_data_frame_t frame = {
        .type = ESP_PEER_DATA_CHANNEL_STRING,
        .stream_id = session->channel_id,
        .data = (uint8_t *)json,
        .size = (int)strlen(json),
    };
    bool sent = esp_peer_send_data(session->peer, &frame) == ESP_PEER_ERR_NONE;
    free(json);
    return sent;
}

static void stream_to_viewer(struct webrtc_camera *session)
{
    /* Control messages go first: they are tiny, and the viewer needs them to render its controls. */
    if (session->capabilities_pending &&
        send_text(session, camera_controls_capabilities_json(&session->controls))) {
        session->capabilities_pending = false;
    }
    if (!session->capabilities_pending && session->state_pending &&
        send_text(session, camera_controls_state_json(&session->controls))) {
        session->state_pending = false;
    }

    session->streamer.frame_interval_ms = camera_controls_frame_interval_ms(&session->controls);
    jpeg_streamer_result_t result = jpeg_streamer_loop(&session->streamer, session->peer, session->channel_id);
    if (result == JPEG_STREAMER_COMPLETED || result == JPEG_STREAMER_ABANDONED) {
        if (result == JPEG_STREAMER_ABANDONED) {
            ESP_LOGW(TAG, "Frame dropped: %u/%u bytes in %" PRIu32 " ms, %" PRIu32 " blocked sends",
                     (unsigned)session->streamer.last_sent_bytes, (unsigned)session->streamer.last_frame_bytes,
                     session->streamer.last_duration_ms, session->streamer.last_blocked_sends);
        }
        camera_controls_on_frame_result(&session->controls, result == JPEG_STREAMER_COMPLETED,
                                        session->streamer.last_duration_ms);
    }

    if (camera_controls_take_state_changed(&session->controls)) {
        session->state_pending = true;
    }
}

static void poll_audio(struct webrtc_camera *session)
{
    if (session->config.audio_source == NULL) {
        return;
    }

    uint8_t pcmu[AUDIO_FRAME_BYTES];
    if (!session->config.audio_source(pcmu, sizeof(pcmu), session->config.audio_ctx)) {
        return;
    }
    if (session->audio_active && atomic_load(&session->channel_open)) {
        esp_peer_audio_frame_t frame = {.pts = session->audio_pts, .data = pcmu, .size = sizeof(pcmu)};
        esp_peer_send_audio(session->peer, &frame);
    }
    session->audio_pts += AUDIO_FRAME_MS;
}

static void session_task(void *arg)
{
    struct webrtc_camera *session = arg;

    for (;;) {
        /* With no viewer and no microphone there is nothing to poll, so wait for a command. */
        bool busy = session->peer != NULL || session->config.audio_source != NULL;
        command_t command;
        TickType_t wait = busy ? 0 : portMAX_DELAY;
        while (xQueueReceive(session->commands, &command, wait) == pdTRUE) {
            if (command.type == COMMAND_START) {
                start_peer(session, &command);
            } else {
                close_peer(session);
            }
            free_command(&command);
            wait = 0;
        }

        poll_audio(session);

        if (session->peer != NULL) {
            esp_peer_main_loop(session->peer);

            uint32_t now = webrtc_camera_now_ms();
            if (!session->answer_published) {
                if (session->local_sdp != NULL && now - session->last_signal_ms >= CANDIDATE_SETTLE_MS) {
                    publish_answer(session, true, NULL);
                } else if (now - session->session_started_ms >= session->config.answer_timeout_ms) {
                    ESP_LOGW(TAG, "No local SDP within %" PRIu32 " ms", session->config.answer_timeout_ms);
                    publish_answer(session, false, "Camera timed out gathering network candidates");
                    session->close_requested = true;
                }
            }

            if (atomic_load(&session->channel_open)) {
                stream_to_viewer(session);
            } else if (session->answer_published &&
                       now - session->session_started_ms > session->config.channel_open_timeout_ms) {
                session->close_requested = true;
            }

            if (session->close_requested) {
                close_peer(session);
            }
        }

        if (session->peer != NULL || session->config.audio_source != NULL) {
            /* One fragment per loop, so the tick length caps throughput; see webrtc_camera_start(). */
            vTaskDelay(1);
        }
    }
}

esp_err_t webrtc_camera_start(const webrtc_camera_config_t *config, webrtc_camera_handle_t *out_handle)
{
    if (config == NULL || out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_handle = NULL;

    if (configTICK_RATE_HZ < 1000) {
        ESP_LOGW(TAG, "CONFIG_FREERTOS_HZ is %d: the streamer sends one fragment per tick and needs 1000",
                 (int)configTICK_RATE_HZ);
    }

    struct webrtc_camera *session = calloc(1, sizeof(*session));
    if (session == NULL) {
        return ESP_ERR_NO_MEM;
    }

    session->config = *config;
    session->answer_published = true;
    atomic_init(&session->channel_open, false);
    jpeg_streamer_init(&session->streamer, config->frame_interval_ms, config->max_frame_bytes);
    camera_controls_init(&session->controls, config->max_frame_size, config->flash_gpio,
                         config->frame_interval_ms, config->auto_quality);

    session->commands = xQueueCreate(4, sizeof(command_t));
    session->answer_ready = xSemaphoreCreateBinary();
    session->answer_lock = xSemaphoreCreateMutex();
    session->offer_lock = xSemaphoreCreateMutex();

    if (session->commands != NULL && session->answer_ready != NULL && session->answer_lock != NULL &&
        session->offer_lock != NULL &&
        xTaskCreate(session_task, "webrtc_camera", config->task_stack_size, session, config->task_priority,
                    &session->task) == pdPASS) {
        *out_handle = session;
        return ESP_OK;
    }

    if (session->commands != NULL) {
        vQueueDelete(session->commands);
    }
    if (session->answer_ready != NULL) {
        vSemaphoreDelete(session->answer_ready);
    }
    if (session->answer_lock != NULL) {
        vSemaphoreDelete(session->answer_lock);
    }
    if (session->offer_lock != NULL) {
        vSemaphoreDelete(session->offer_lock);
    }
    free(session);
    return ESP_ERR_NO_MEM;
}

esp_err_t webrtc_camera_handle_offer(webrtc_camera_handle_t handle, const char *offer_sdp,
                                     const webrtc_camera_ice_server_t *ice_servers, size_t ice_server_count,
                                     char **answer_sdp, char *error, size_t error_size)
{
    if (handle == NULL || offer_sdp == NULL || answer_sdp == NULL ||
        (ice_server_count > 0 && ice_servers == NULL)) {
        set_error(error, error_size, "Invalid WebRTC offer");
        return ESP_ERR_INVALID_ARG;
    }
    *answer_sdp = NULL;
    set_error(error, error_size, "");

    if (strncmp(offer_sdp, "v=0", 3) != 0 || strlen(offer_sdp) > MAX_SIGNAL_BYTES) {
        set_error(error, error_size, "Invalid WebRTC offer");
        return ESP_ERR_INVALID_ARG;
    }
    if (ice_server_count > MAX_ICE_SERVERS) {
        ESP_LOGW(TAG, "Using the first %d of %u ICE servers", MAX_ICE_SERVERS, (unsigned)ice_server_count);
        ice_server_count = MAX_ICE_SERVERS;
    }

    xSemaphoreTake(handle->offer_lock, portMAX_DELAY);

    command_t command = {
        .type = COMMAND_START,
        .sequence = ++handle->sequence,
        .offer = strdup(offer_sdp),
        .servers = ice_server_count > 0 ? copy_servers(ice_servers, ice_server_count) : NULL,
        .server_count = ice_server_count,
    };
    /* Drop a completion left over from an earlier offer that timed out on this side. */
    xSemaphoreTake(handle->answer_ready, 0);

    if (command.offer == NULL || (ice_server_count > 0 && command.servers == NULL)) {
        free_command(&command);
        set_error(error, error_size, "Camera is out of memory");
        xSemaphoreGive(handle->offer_lock);
        return ESP_ERR_NO_MEM;
    }
    if (xQueueSend(handle->commands, &command, 0) != pdTRUE) {
        free_command(&command);
        set_error(error, error_size, "Camera WebRTC session is busy");
        xSemaphoreGive(handle->offer_lock);
        return ESP_ERR_INVALID_STATE;
    }

    bool answered = false;
    bool ok = false;
    char *answer = NULL;
    const TickType_t budget = pdMS_TO_TICKS(handle->config.answer_timeout_ms + ANSWER_WAIT_SLACK_MS);
    const TickType_t started = xTaskGetTickCount();

    while (!answered) {
        TickType_t elapsed = xTaskGetTickCount() - started;
        if (elapsed >= budget || xSemaphoreTake(handle->answer_ready, budget - elapsed) != pdTRUE) {
            break;
        }

        xSemaphoreTake(handle->answer_lock, portMAX_DELAY);
        if (handle->answer_sequence == command.sequence) {
            answered = true;
            ok = handle->answer_ok;
            answer = handle->answer;
            handle->answer = NULL;
            if (!ok) {
                set_error(error, error_size, handle->answer_error);
            }
        }
        xSemaphoreGive(handle->answer_lock);
    }

    if (!answered) {
        set_error(error, error_size, "Camera timed out creating the WebRTC answer");
    } else if (ok && (answer == NULL || answer[0] == '\0')) {
        set_error(error, error_size, "Camera produced an empty WebRTC answer");
        ok = false;
    }
    if (!ok) {
        free(answer);
        answer = NULL;
        append_weak_signal(error, error_size);
    }

    xSemaphoreGive(handle->offer_lock);

    if (!ok) {
        return answered ? ESP_FAIL : ESP_ERR_TIMEOUT;
    }
    *answer_sdp = answer;
    return ESP_OK;
}

void webrtc_camera_stop_viewer(webrtc_camera_handle_t handle)
{
    if (handle == NULL) {
        return;
    }
    command_t command = {.type = COMMAND_STOP};
    xQueueSend(handle->commands, &command, 0);
}

bool webrtc_camera_is_streaming(webrtc_camera_handle_t handle)
{
    return handle != NULL && atomic_load(&handle->channel_open);
}
