/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "mode_controller.h"
#include "../core/sinricpro_device_internal.h"
#include "../core/sinricpro_event_limiter.h"
#include "sinricpro_types.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "mode_ctrl";

struct sinricpro_mode_controller {
    sinricpro_mode_callback_t callback;
    void *user_data;
    sinricpro_event_limiter_handle_t limiter;
};

sinricpro_mode_controller_handle_t sinricpro_mode_controller_create(void)
{
    sinricpro_mode_controller_handle_t handle =
        malloc(sizeof(struct sinricpro_mode_controller));

    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate ModeController");
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

    ESP_LOGD(TAG, "ModeController created");
    return handle;
}

esp_err_t sinricpro_mode_controller_set_callback(
    sinricpro_mode_controller_handle_t handle,
    sinricpro_mode_callback_t callback,
    void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->callback = callback;
    handle->user_data = user_data;
    ESP_LOGD(TAG, "Mode callback registered");
    return ESP_OK;
}

bool sinricpro_mode_controller_handle_request(
    sinricpro_mode_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value)
{
    if (handle == NULL || action == NULL) {
        return false;
    }

    if (strcmp(action, "setMode") == 0) {
        if (handle->callback == NULL) {
            ESP_LOGW(TAG, "No mode callback registered");
            return false;
        }

        cJSON *mode_item = cJSON_GetObjectItem(request_value, "mode");
        if (mode_item == NULL || !cJSON_IsString(mode_item)) {
            ESP_LOGE(TAG, "Invalid mode in request");
            return false;
        }

        const char *mode = mode_item->valuestring;
        ESP_LOGI(TAG, "setMode: device=%s, mode=%s", device_id, mode);

        bool success = handle->callback(device_id, &mode, handle->user_data);
        cJSON_AddStringToObject(response_value, "mode", mode);
        return success;
    }

    return false;
}

esp_err_t sinricpro_mode_controller_send_event(
    sinricpro_mode_controller_handle_t handle,
    const char *device_id,
    const char *mode,
    const char *cause)
{
    if (handle == NULL || device_id == NULL || mode == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sinricpro_event_limiter_check(handle->limiter)) {
        uint32_t wait_ms = sinricpro_event_limiter_time_until_next(handle->limiter);
        ESP_LOGW(TAG, "Mode event rate limited (wait %lu ms)", wait_ms);
        return SINRICPRO_ERR_RATE_LIMITED;
    }

    ESP_LOGI(TAG, "Sending mode event: device=%s, mode=%s, cause=%s",
             device_id, mode, cause);

    cJSON *value = cJSON_CreateObject();
    cJSON_AddStringToObject(value, "mode", mode);

    esp_err_t ret = sinricpro_core_send_event(device_id, "setMode", cause, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send mode event: %s", esp_err_to_name(ret));
    }

    return ret;
}

void sinricpro_mode_controller_destroy(sinricpro_mode_controller_handle_t handle)
{
    if (handle == NULL) return;
    if (handle->limiter) sinricpro_event_limiter_destroy(handle->limiter);
    free(handle);
    ESP_LOGD(TAG, "ModeController destroyed");
}
