/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "camera_controller.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "mbedtls/base64.h"

static const char *TAG = "camera_ctrl";

#define ACTION_GET_CAMERA_CAPABILITIES "getCameraCapabilities"
#define ACTION_GET_WEBRTC_ANSWER       "getWebRTCAnswer"

/* The server sends one STUN URL and a few TURN URLs; this only bounds a malformed list. */
#define MAX_ICE_SERVER_URLS 16

/**
 * @brief CameraController context
 */
struct sinricpro_camera_controller {
    sinricpro_camera_webrtc_offer_callback_t offer_callback;
    void *offer_user_data;
    bool webrtc_audio;
};

sinricpro_camera_controller_handle_t sinricpro_camera_controller_create(void)
{
    sinricpro_camera_controller_handle_t handle = calloc(1, sizeof(struct sinricpro_camera_controller));
    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate CameraController");
        return NULL;
    }

    ESP_LOGD(TAG, "CameraController created");
    return handle;
}

esp_err_t sinricpro_camera_controller_set_webrtc_offer_callback(
    sinricpro_camera_controller_handle_t handle,
    sinricpro_camera_webrtc_offer_callback_t callback,
    void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->offer_callback = callback;
    handle->offer_user_data = user_data;
    return ESP_OK;
}

void sinricpro_camera_controller_set_webrtc_audio(sinricpro_camera_controller_handle_t handle,
                                                  bool enabled)
{
    if (handle != NULL) {
        handle->webrtc_audio = enabled;
    }
}

bool sinricpro_camera_controller_owns_action(const char *action)
{
    return action != NULL &&
           (strcmp(action, ACTION_GET_CAMERA_CAPABILITIES) == 0 ||
            strcmp(action, ACTION_GET_WEBRTC_ANSWER) == 0);
}

static char *base64_decode_string(const char *input)
{
    size_t input_len = strlen(input);
    size_t output_len = 0;

    /* Given no buffer, mbedTLS reports the size it needs through output_len. */
    int ret = mbedtls_base64_decode(NULL, 0, &output_len, (const unsigned char *)input, input_len);
    if (ret != 0 && ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
        return NULL;
    }

    char *output = malloc(output_len + 1);
    if (output == NULL) {
        return NULL;
    }
    if (mbedtls_base64_decode((unsigned char *)output, output_len, &output_len,
                              (const unsigned char *)input, input_len) != 0) {
        free(output);
        return NULL;
    }
    output[output_len] = '\0';
    return output;
}

static char *base64_encode_string(const char *input)
{
    size_t input_len = strlen(input);
    size_t output_len = 0;

    /* The size reported includes the terminating NUL that mbedTLS writes. */
    mbedtls_base64_encode(NULL, 0, &output_len, (const unsigned char *)input, input_len);
    if (output_len == 0) {
        return NULL;
    }

    char *output = malloc(output_len);
    if (output == NULL) {
        return NULL;
    }
    size_t written = 0;
    if (mbedtls_base64_encode((unsigned char *)output, output_len, &written,
                              (const unsigned char *)input, input_len) != 0) {
        free(output);
        return NULL;
    }
    return output;
}

/* The server sends RTCIceServer entries whose "urls" is either a string or an
 * array. Each URL becomes one flat entry, so callbacks handle a single shape. */
static size_t collect_ice_servers(cJSON *list, sinricpro_ice_server_t *out, size_t max)
{
    size_t count = 0;
    cJSON *server = NULL;

    cJSON_ArrayForEach(server, list) {
        if (!cJSON_IsObject(server)) {
            continue;
        }

        cJSON *username = cJSON_GetObjectItem(server, "username");
        cJSON *credential = cJSON_GetObjectItem(server, "credential");
        const char *user = cJSON_IsString(username) ? username->valuestring : "";
        const char *secret = cJSON_IsString(credential) ? credential->valuestring : "";

        cJSON *urls = cJSON_GetObjectItem(server, "urls");
        if (cJSON_IsString(urls)) {
            if (count < max) {
                out[count++] = (sinricpro_ice_server_t){urls->valuestring, user, secret};
            }
            continue;
        }

        cJSON *url = NULL;
        cJSON_ArrayForEach(url, urls) {
            if (cJSON_IsString(url) && count < max) {
                out[count++] = (sinricpro_ice_server_t){url->valuestring, user, secret};
            }
        }
    }

    return count;
}

static bool handle_webrtc_offer(sinricpro_camera_controller_handle_t handle,
                                const char *device_id,
                                cJSON *request_value,
                                cJSON *response_value)
{
    if (handle->offer_callback == NULL) {
        ESP_LOGW(TAG, "No WebRTC offer callback registered");
        return false;
    }

    cJSON *offer_item = cJSON_GetObjectItem(request_value, "offer");
    if (!cJSON_IsString(offer_item)) {
        ESP_LOGE(TAG, "getWebRTCAnswer request has no offer");
        return false;
    }

    char *offer = base64_decode_string(offer_item->valuestring);
    if (offer == NULL) {
        ESP_LOGE(TAG, "WebRTC offer is not valid base64");
        return false;
    }

    sinricpro_ice_server_t servers[MAX_ICE_SERVER_URLS];
    size_t server_count = collect_ice_servers(cJSON_GetObjectItem(request_value, "iceServers"),
                                              servers, MAX_ICE_SERVER_URLS);

    char *answer_sdp = NULL;
    bool success = handle->offer_callback(device_id, offer, servers, server_count,
                                          &answer_sdp, handle->offer_user_data);
    free(offer);

    if (success) {
        char *answer = (answer_sdp != NULL && answer_sdp[0] != '\0') ? base64_encode_string(answer_sdp) : NULL;
        if (answer != NULL) {
            cJSON_AddStringToObject(response_value, "answer", answer);
            free(answer);
        } else {
            ESP_LOGE(TAG, "WebRTC offer callback reported success without an answer");
            success = false;
        }
    }

    free(answer_sdp);
    return success;
}

bool sinricpro_camera_controller_handle_request(
    sinricpro_camera_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value)
{
    if (handle == NULL || action == NULL) {
        return false;
    }

    if (strcmp(action, ACTION_GET_CAMERA_CAPABILITIES) == 0) {
        /* Viewers ask before connecting, so firmware without a WebRTC callback is
         * told apart from one that is merely unreachable. */
        bool webrtc = handle->offer_callback != NULL;
        cJSON_AddBoolToObject(response_value, "webrtc", webrtc);
        cJSON_AddBoolToObject(response_value, "webrtcAudio", webrtc && handle->webrtc_audio);
        return true;
    }

    if (strcmp(action, ACTION_GET_WEBRTC_ANSWER) == 0) {
        return handle_webrtc_offer(handle, device_id, request_value, response_value);
    }

    return false;
}

void sinricpro_camera_controller_destroy(sinricpro_camera_controller_handle_t handle)
{
    free(handle);
}
