/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "media_controller.h"
#include "../core/sinricpro_device_internal.h"
#include "../core/sinricpro_event_limiter.h"
#include "sinricpro_types.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "media_ctrl";

struct sinricpro_media_controller {
    sinricpro_media_control_callback_t callback;
    void *user_data;
    sinricpro_event_limiter_handle_t limiter;
};

sinricpro_media_controller_handle_t sinricpro_media_controller_create(void)
{
    sinricpro_media_controller_handle_t handle =
        malloc(sizeof(struct sinricpro_media_controller));

    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate MediaController");
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

    ESP_LOGD(TAG, "MediaController created");
    return handle;
}

esp_err_t sinricpro_media_controller_set_callback(
    sinricpro_media_controller_handle_t handle,
    sinricpro_media_control_callback_t callback,
    void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->callback = callback;
    handle->user_data = user_data;
    ESP_LOGD(TAG, "Media control callback registered");
    return ESP_OK;
}

bool sinricpro_media_controller_handle_request(
    sinricpro_media_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value)
{
    if (handle == NULL || action == NULL) {
        return false;
    }

    if (strcmp(action, "mediaControl") == 0) {
        if (handle->callback == NULL) {
            ESP_LOGW(TAG, "No media control callback registered");
            return false;
        }

        cJSON *control_item = cJSON_GetObjectItem(request_value, "control");
        if (control_item == NULL || !cJSON_IsString(control_item)) {
            ESP_LOGE(TAG, "Invalid control in request");
            return false;
        }

        const char *control = control_item->valuestring;
        ESP_LOGI(TAG, "mediaControl: device=%s, control=%s", device_id, control);

        bool success = handle->callback(device_id, control, handle->user_data);
        cJSON_AddStringToObject(response_value, "control", control);
        return success;
    }

    return false;
}

esp_err_t sinricpro_media_controller_send_event(
    sinricpro_media_controller_handle_t handle,
    const char *device_id,
    const char *control,
    const char *cause)
{
    if (handle == NULL || device_id == NULL || control == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sinricpro_event_limiter_check(handle->limiter)) {
        uint32_t wait_ms = sinricpro_event_limiter_time_until_next(handle->limiter);
        ESP_LOGW(TAG, "Media control event rate limited (wait %lu ms)", wait_ms);
        return SINRICPRO_ERR_RATE_LIMITED;
    }

    ESP_LOGI(TAG, "Sending media control event: device=%s, control=%s, cause=%s",
             device_id, control, cause);

    cJSON *value = cJSON_CreateObject();
    cJSON_AddStringToObject(value, "control", control);

    esp_err_t ret = sinricpro_core_send_event(device_id, "mediaControl", cause, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send media control event: %s", esp_err_to_name(ret));
    }

    return ret;
}

void sinricpro_media_controller_destroy(sinricpro_media_controller_handle_t handle)
{
    if (handle == NULL) return;
    if (handle->limiter) sinricpro_event_limiter_destroy(handle->limiter);
    free(handle);
    ESP_LOGD(TAG, "MediaController destroyed");
}
