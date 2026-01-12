/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "mute_controller.h"
#include "../core/sinricpro_device_internal.h"
#include "../core/sinricpro_event_limiter.h"
#include "sinricpro_types.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "mute_ctrl";

struct sinricpro_mute_controller {
    sinricpro_mute_callback_t callback;
    void *user_data;
    sinricpro_event_limiter_handle_t limiter;
};

sinricpro_mute_controller_handle_t sinricpro_mute_controller_create(void)
{
    sinricpro_mute_controller_handle_t handle =
        malloc(sizeof(struct sinricpro_mute_controller));

    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate MuteController");
        return NULL;
    }

    handle->callback = NULL;
    handle->user_data = NULL;
    handle->limiter = sinricpro_event_limiter_create(SINRICPRO_EVENT_LIMIT_STATE);

    if (handle->limiter == NULL) {
        ESP_LOGE(TAG, "Failed to create event limiter");
        free(handle);
        return NULL;
    }

    ESP_LOGD(TAG, "MuteController created");
    return handle;
}

esp_err_t sinricpro_mute_controller_set_callback(
    sinricpro_mute_controller_handle_t handle,
    sinricpro_mute_callback_t callback,
    void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->callback = callback;
    handle->user_data = user_data;
    ESP_LOGD(TAG, "Mute callback registered");
    return ESP_OK;
}

bool sinricpro_mute_controller_handle_request(
    sinricpro_mute_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value)
{
    if (handle == NULL || action == NULL) {
        return false;
    }

    if (strcmp(action, "setMute") == 0) {
        if (handle->callback == NULL) {
            ESP_LOGW(TAG, "No setMute callback registered");
            return false;
        }

        cJSON *mute_item = cJSON_GetObjectItem(request_value, "mute");
        if (mute_item == NULL || !cJSON_IsBool(mute_item)) {
            ESP_LOGE(TAG, "Invalid mute in request");
            return false;
        }

        bool mute = cJSON_IsTrue(mute_item);
        ESP_LOGI(TAG, "setMute: device=%s, mute=%s", device_id, mute ? "true" : "false");

        bool success = handle->callback(device_id, &mute, handle->user_data);
        cJSON_AddBoolToObject(response_value, "mute", mute);
        return success;
    }

    return false;
}

esp_err_t sinricpro_mute_controller_send_event(
    sinricpro_mute_controller_handle_t handle,
    const char *device_id,
    bool mute,
    const char *cause)
{
    if (handle == NULL || device_id == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sinricpro_event_limiter_check(handle->limiter)) {
        uint32_t wait_ms = sinricpro_event_limiter_time_until_next(handle->limiter);
        ESP_LOGW(TAG, "Mute event rate limited (wait %lu ms)", wait_ms);
        return SINRICPRO_ERR_RATE_LIMITED;
    }

    ESP_LOGI(TAG, "Sending mute event: device=%s, mute=%s, cause=%s",
             device_id, mute ? "true" : "false", cause);

    cJSON *value = cJSON_CreateObject();
    cJSON_AddBoolToObject(value, "mute", mute);

    esp_err_t ret = sinricpro_core_send_event(device_id, "setMute", cause, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send mute event: %s", esp_err_to_name(ret));
    }

    return ret;
}

void sinricpro_mute_controller_destroy(sinricpro_mute_controller_handle_t handle)
{
    if (handle == NULL) return;
    if (handle->limiter) sinricpro_event_limiter_destroy(handle->limiter);
    free(handle);
    ESP_LOGD(TAG, "MuteController destroyed");
}
