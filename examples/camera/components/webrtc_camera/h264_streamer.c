/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "h264_streamer.h"

#include <stdlib.h>
#include <string.h>

#include "esp_camera.h"
#include "esp_h264_alloc.h"
#include "esp_h264_enc_param.h"
#include "esp_h264_enc_single.h"
#if CONFIG_IDF_TARGET_ESP32P4
#include "esp_h264_enc_param_hw.h"
#include "esp_h264_enc_single_hw.h"
#include "driver/ppa.h"
#else
#include "esp_h264_enc_single_sw.h"
#endif
#include "esp_log.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "webrtc_camera_priv.h"

static const char *TAG = "h264_streamer";

/* Two slots: the session task sends one while the encoder fills the other. */
#define SLOT_COUNT               2
#define KEYFRAME_MIN_INTERVAL_MS 2000
#define STOP_POLL_MS             50
#define LOG_INTERVAL_MS          5000
/* Quantiser floor and ceiling. The floor stops a detailed scene spending the whole bitrate on
 * one frame; the ceiling keeps a moving scene from turning to mush. */
#define QP_MIN                   26
#define QP_MAX                   40

typedef struct {
    uint8_t *data;
    uint32_t capacity;
    uint32_t size;
    uint32_t pts;
} slot_t;

struct h264_streamer {
    h264_streamer_config_t config;
    esp_h264_enc_handle_t encoder;
    esp_h264_enc_param_handle_t params;
    TaskHandle_t task;
    SemaphoreHandle_t stopped;
    QueueHandle_t free_slots;
    QueueHandle_t ready_slots;
    slot_t slots[SLOT_COUNT];
    /* Encoder input: the PPA's YUV output on the P4, an aligned copy of an unaligned buffer on the S3. */
    uint8_t *staging;
    uint32_t staging_size;
#if CONFIG_IDF_TARGET_ESP32P4
    ppa_client_handle_t ppa;
#endif

    volatile bool running;
    volatile bool keyframe_requested;
    volatile uint8_t fps;
    uint32_t started_ms;
    uint32_t last_frame_ms;
    uint32_t last_keyframe_ms;
    uint32_t encoded;
    uint32_t dropped;
    uint32_t last_log_ms;
    uint32_t log_frames;
    uint32_t log_encode_ms;
};

static void close_encoder(h264_streamer_t *streamer)
{
    if (streamer->encoder != NULL) {
        esp_h264_enc_close(streamer->encoder);
        esp_h264_enc_del(streamer->encoder);
        streamer->encoder = NULL;
        streamer->params = NULL;
    }
}

static esp_err_t open_encoder(h264_streamer_t *streamer)
{
    uint8_t fps = streamer->fps > 0 ? streamer->fps : 1;
#if CONFIG_IDF_TARGET_ESP32P4
    /* The only input the encoder takes on rev < 3 silicon; filled from the ISP's BGR888 by ppa_convert() */
    esp_h264_enc_cfg_hw_t cfg = {
        .pic_type = ESP_H264_RAW_FMT_O_UYY_E_VYY,
#else
    esp_h264_enc_cfg_sw_t cfg = {
        .pic_type = ESP_H264_RAW_FMT_YUYV,
#endif
        /* One keyframe per second: a viewer that joins late or loses packets recovers quickly,
         * which matters because the encoder is only reset, not asked, for one on demand. */
        .gop = fps,
        .fps = fps,
        .res = {.width = streamer->config.width, .height = streamer->config.height},
        .rc = {.bitrate = streamer->config.bitrate, .qp_min = QP_MIN, .qp_max = QP_MAX},
    };

#if CONFIG_IDF_TARGET_ESP32P4
    esp_h264_err_t err = esp_h264_enc_hw_new(&cfg, &streamer->encoder);
#else
    esp_h264_err_t err = esp_h264_enc_sw_new(&cfg, &streamer->encoder);
#endif
    if (err != ESP_H264_ERR_OK) {
        ESP_LOGE(TAG, "Encoder allocation failed for %ux%u", (unsigned)cfg.res.width, (unsigned)cfg.res.height);
        streamer->encoder = NULL;
        return ESP_ERR_NO_MEM;
    }
#if CONFIG_IDF_TARGET_ESP32P4
    esp_h264_enc_param_hw_handle_t hw_params = NULL;
    err = esp_h264_enc_hw_get_param_hd(streamer->encoder, &hw_params);
    streamer->params = hw_params != NULL ? &hw_params->base : NULL;
#else
    err = esp_h264_enc_sw_get_param_hd(streamer->encoder, &streamer->params);
#endif
    if (err != ESP_H264_ERR_OK || esp_h264_enc_open(streamer->encoder) != ESP_H264_ERR_OK) {
        ESP_LOGE(TAG, "Encoder open failed");
        close_encoder(streamer);
        return ESP_FAIL;
    }

    streamer->last_keyframe_ms = webrtc_camera_now_ms();
    return ESP_OK;
}

#if !CONFIG_IDF_TARGET_ESP32P4
/* openh264 reads the input plane in 16-byte steps, so an unaligned camera buffer is copied. */
static uint8_t *aligned_input(h264_streamer_t *streamer, const camera_fb_t *frame)
{
    if (((uintptr_t)frame->buf & 15) == 0) {
        return frame->buf;
    }
    if (streamer->staging == NULL || streamer->staging_size < frame->len) {
        esp_h264_free(streamer->staging);
        streamer->staging_size = 0;
        streamer->staging = esp_h264_aligned_calloc(16, 1, frame->len, &streamer->staging_size, MALLOC_CAP_SPIRAM);
        if (streamer->staging == NULL) {
            return NULL;
        }
    }
    memcpy(streamer->staging, frame->buf, frame->len);
    return streamer->staging;
}
#endif

static bool encode_input(h264_streamer_t *streamer, slot_t *slot, uint8_t *input, uint32_t len, uint32_t pts)
{
    esp_h264_enc_in_frame_t in = {
        .raw_data = {.buffer = input, .len = len},
        .pts = pts,
    };
    esp_h264_enc_out_frame_t out = {
        .raw_data = {.buffer = slot->data, .len = slot->capacity},
    };
    esp_h264_err_t err = esp_h264_enc_process(streamer->encoder, &in, &out);
    if (err != ESP_H264_ERR_OK || out.length == 0 || out.length > slot->capacity) {
        ESP_LOGW(TAG, "Encode failed: %d, %u bytes", (int)err, (unsigned)out.length);
        return false;
    }
    slot->size = out.length;
    slot->pts = pts;
    return true;
}

#if CONFIG_IDF_TARGET_ESP32P4
/* The PPA writes through the L2 cache, so its output buffer and length align to that line size */
#define PPA_ALIGN 128

static bool ppa_prepare(h264_streamer_t *streamer, uint32_t len)
{
    if (streamer->ppa == NULL) {
        ppa_client_config_t client = {.oper_type = PPA_OPERATION_SRM, .max_pending_trans_num = 1};
        if (ppa_register_client(&client, &streamer->ppa) != ESP_OK) {
            streamer->ppa = NULL;
            return false;
        }
    }
    const uint32_t alloc_len = (len + PPA_ALIGN - 1) & ~(uint32_t)(PPA_ALIGN - 1);
    if (streamer->staging == NULL || streamer->staging_size < alloc_len) {
        esp_h264_free(streamer->staging);
        streamer->staging_size = 0;
        streamer->staging = esp_h264_aligned_calloc(PPA_ALIGN, 1, alloc_len, &streamer->staging_size, MALLOC_CAP_SPIRAM);
    }
    return streamer->staging != NULL;
}

/*
 * BGR888 to packed YUV420 (limited range BT.601) on the PPA. This is the only input the encoder
 * takes on rev < 3 silicon; the ISP can output the layout itself, but there its chroma decodes
 * green and nearly colourless. Conversion and encoding run back to back rather than overlapped:
 * both are PSRAM bound, so overlapping saved only ~5 ms per HD frame.
 */
static bool ppa_convert(h264_streamer_t *streamer, const camera_fb_t *frame)
{
    ppa_srm_oper_config_t op = {
        .in = {
            .buffer = frame->buf, .pic_w = frame->width, .pic_h = frame->height,
            .block_w = frame->width, .block_h = frame->height,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB888,
        },
        .out = {
            .buffer = streamer->staging, .buffer_size = streamer->staging_size,
            .pic_w = frame->width, .pic_h = frame->height,
            .srm_cm = PPA_SRM_COLOR_MODE_YUV420,
            .yuv_range = PPA_COLOR_RANGE_LIMIT,
            .yuv_std = PPA_COLOR_CONV_STD_RGB_YUV_BT601,
        },
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
        .scale_x = 1.0f,
        .scale_y = 1.0f,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };
    esp_err_t err = ppa_do_scale_rotate_mirror(streamer->ppa, &op);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "PPA conversion failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

static bool encode_frame(h264_streamer_t *streamer, slot_t *slot)
{
    camera_fb_t *frame = esp_camera_fb_get();
    if (frame == NULL) {
        return false;
    }
    if (frame->format != PIXFORMAT_RGB888) {
        ESP_LOGW(TAG, "Camera is not in the encoder's input mode");
        esp_camera_fb_return(frame);
        return false;
    }
    // The encoder is built for the mode's size; a sensor whose native modes differ cannot feed it
    if (frame->width != streamer->config.width || frame->height != streamer->config.height) {
        ESP_LOGE(TAG, "Camera delivers %ux%u, the encoder expects %ux%u", (unsigned)frame->width,
                 (unsigned)frame->height, (unsigned)streamer->config.width, (unsigned)streamer->config.height);
        esp_camera_fb_return(frame);
        return false;
    }

    const uint32_t len = (uint32_t)(frame->width * frame->height * 3 / 2);
    const uint32_t pts = webrtc_camera_now_ms() - streamer->started_ms;
    bool converted = ppa_prepare(streamer, len) && ppa_convert(streamer, frame);
    // The converted copy is all the encoder needs, so the camera gets its buffer back early
    esp_camera_fb_return(frame);
    return converted && encode_input(streamer, slot, streamer->staging, len, pts);
}
#else
static bool encode_frame(h264_streamer_t *streamer, slot_t *slot)
{
    camera_fb_t *frame = esp_camera_fb_get();
    if (frame == NULL) {
        return false;
    }

    bool encoded = false;
    if (frame->format != PIXFORMAT_YUV422) {
        ESP_LOGW(TAG, "Camera is not in the encoder's input mode");
    } else {
        uint8_t *input = aligned_input(streamer, frame);
        if (input != NULL) {
            encoded = encode_input(streamer, slot, input, frame->len,
                                   webrtc_camera_now_ms() - streamer->started_ms);
        }
    }
    esp_camera_fb_return(frame);
    return encoded;
}
#endif

static void encode_task(void *arg)
{
    h264_streamer_t *streamer = arg;

    while (streamer->running) {
        uint8_t fps = streamer->fps > 0 ? streamer->fps : 1;
        uint32_t interval = 1000 / fps;
        uint32_t now = webrtc_camera_now_ms();
        if (now - streamer->last_frame_ms < interval) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        /* A viewer that cannot keep up leaves both slots outstanding; the wait doubles as the
         * back pressure that keeps the encoder from running ahead of the link. */
        slot_t *slot = NULL;
        if (xQueueReceive(streamer->free_slots, &slot, pdMS_TO_TICKS(STOP_POLL_MS)) != pdTRUE) {
            continue;
        }
        streamer->last_frame_ms = now;

        if (streamer->keyframe_requested) {
            streamer->keyframe_requested = false;
            if (now - streamer->last_keyframe_ms >= KEYFRAME_MIN_INTERVAL_MS) {
                close_encoder(streamer);
                if (open_encoder(streamer) != ESP_OK) {
                    streamer->running = false;
                }
            }
        }

        if (streamer->encoder != NULL && encode_frame(streamer, slot)) {
            streamer->encoded++;
            xQueueSend(streamer->ready_slots, &slot, 0);
        } else {
            streamer->dropped++;
            xQueueSend(streamer->free_slots, &slot, 0);
        }

        streamer->log_frames++;
        streamer->log_encode_ms += webrtc_camera_now_ms() - now;
        if (now - streamer->last_log_ms >= LOG_INTERVAL_MS) {
            ESP_LOGI(TAG, "%u frames in %u ms, %u ms each, %u dropped so far",
                     (unsigned)streamer->log_frames, (unsigned)(now - streamer->last_log_ms),
                     (unsigned)(streamer->log_encode_ms / streamer->log_frames),
                     (unsigned)streamer->dropped);
            streamer->last_log_ms = now;
            streamer->log_frames = 0;
            streamer->log_encode_ms = 0;
        }

        /* Encoding occupies this core for nearly the whole frame interval, leaving the pacing
         * delay above unreachable. Without an explicit tick the priority-0 idle task never runs
         * and the task watchdog kills the session after five seconds. */
        vTaskDelay(1);
    }

    xSemaphoreGive(streamer->stopped);
    vTaskDelete(NULL);
}

static void free_streamer(h264_streamer_t *streamer)
{
    close_encoder(streamer);
    for (size_t i = 0; i < SLOT_COUNT; i++) {
        esp_h264_free(streamer->slots[i].data);
    }
    esp_h264_free(streamer->staging);
#if CONFIG_IDF_TARGET_ESP32P4
    if (streamer->ppa != NULL) {
        ppa_unregister_client(streamer->ppa);
    }
#endif
    if (streamer->free_slots != NULL) {
        vQueueDelete(streamer->free_slots);
    }
    if (streamer->ready_slots != NULL) {
        vQueueDelete(streamer->ready_slots);
    }
    if (streamer->stopped != NULL) {
        vSemaphoreDelete(streamer->stopped);
    }
    free(streamer);
}

esp_err_t h264_streamer_start(const h264_streamer_config_t *config, h264_streamer_t **out_streamer)
{
    if (config == NULL || out_streamer == NULL || config->width == 0 || config->height == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_streamer = NULL;

    h264_streamer_t *streamer = calloc(1, sizeof(*streamer));
    if (streamer == NULL) {
        return ESP_ERR_NO_MEM;
    }
    streamer->config = *config;
    streamer->fps = config->fps > 0 ? config->fps : 10;
    streamer->started_ms = webrtc_camera_now_ms();
    streamer->last_log_ms = streamer->started_ms;

    streamer->free_slots = xQueueCreate(SLOT_COUNT, sizeof(slot_t *));
    streamer->ready_slots = xQueueCreate(SLOT_COUNT, sizeof(slot_t *));
    streamer->stopped = xSemaphoreCreateBinary();
    if (streamer->free_slots == NULL || streamer->ready_slots == NULL || streamer->stopped == NULL) {
        free_streamer(streamer);
        return ESP_ERR_NO_MEM;
    }

    /* One byte per pixel holds an IDR comfortably at these resolutions, and P-frames use a
     * fraction of it. Both buffers live in PSRAM; only the RTP copy touches internal RAM. */
    uint32_t capacity = (uint32_t)config->width * config->height;
    for (size_t i = 0; i < SLOT_COUNT; i++) {
        slot_t *slot = &streamer->slots[i];
        slot->data = esp_h264_aligned_calloc(16, 1, capacity, &slot->capacity, MALLOC_CAP_SPIRAM);
        if (slot->data == NULL) {
            free_streamer(streamer);
            return ESP_ERR_NO_MEM;
        }
        xQueueSend(streamer->free_slots, &slot, 0);
    }

    if (open_encoder(streamer) != ESP_OK) {
        free_streamer(streamer);
        return ESP_ERR_NO_MEM;
    }

    streamer->running = true;
    /* Pinned away from core 0, where the Wi-Fi driver and the session task do their work. */
    if (xTaskCreatePinnedToCore(encode_task, "h264_enc", config->task_stack_size, streamer,
                                config->task_priority, &streamer->task, 1) != pdPASS) {
        streamer->running = false;
        free_streamer(streamer);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "H.264 %ux%u at %u fps, %u bps", (unsigned)config->width, (unsigned)config->height,
             (unsigned)streamer->fps, (unsigned)config->bitrate);
    *out_streamer = streamer;
    return ESP_OK;
}

void h264_streamer_stop(h264_streamer_t *streamer)
{
    if (streamer == NULL) {
        return;
    }
    streamer->running = false;
    /* The task holds a camera frame buffer while encoding, so it must finish before the caller
     * reconfigures the camera. */
    xSemaphoreTake(streamer->stopped, pdMS_TO_TICKS(2 * STOP_POLL_MS + 1000));
    ESP_LOGI(TAG, "Stopped after %u frames, %u dropped", (unsigned)streamer->encoded,
             (unsigned)streamer->dropped);
    free_streamer(streamer);
}

bool h264_streamer_send(h264_streamer_t *streamer, esp_peer_handle_t peer)
{
    slot_t *slot = NULL;
    if (streamer == NULL || xQueueReceive(streamer->ready_slots, &slot, 0) != pdTRUE) {
        return false;
    }

    esp_peer_video_frame_t frame = {.pts = slot->pts, .data = slot->data, .size = (int)slot->size};
    int ret = esp_peer_send_video(peer, &frame);
    if (ret != ESP_PEER_ERR_NONE) {
        streamer->dropped++;
    }
    xQueueSend(streamer->free_slots, &slot, 0);
    return ret == ESP_PEER_ERR_NONE;
}

void h264_streamer_request_keyframe(h264_streamer_t *streamer)
{
    if (streamer != NULL) {
        streamer->keyframe_requested = true;
    }
}

void h264_streamer_set_fps(h264_streamer_t *streamer, uint8_t fps)
{
    if (streamer == NULL || fps == 0 || fps == streamer->fps) {
        return;
    }
    streamer->fps = fps;
    /* The encoder's rate control needs the same frame rate the capture loop uses, or it spends
     * the bitrate as though frames arrived faster than they do. */
    if (streamer->params != NULL) {
        esp_h264_enc_set_fps(streamer->params, fps);
        esp_h264_enc_set_gop(streamer->params, fps);
    }
}

void h264_streamer_stats(const h264_streamer_t *streamer, uint32_t *encoded, uint32_t *dropped)
{
    if (streamer == NULL) {
        return;
    }
    if (encoded != NULL) {
        *encoded = streamer->encoded;
    }
    if (dropped != NULL) {
        *dropped = streamer->dropped;
    }
}
