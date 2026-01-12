/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "power_level_controller.h"
#include "../core/sinricpro_device_internal.h"
#include "../core/sinricpro_event_limiter.h"
#include "sinricpro_types.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "power_level_ctrl";

struct sinricpro_power_level_controller {
    sinricpro_power_level_callback_t callback;
    sinricpro_adjust_power_level_callback_t adjust_callback;
    void *user_data;
    void *adjust_user_data;
    sinricpro_event_limiter_handle_t limiter;
};

sinricpro_power_level_controller_handle_t sinricpro_power_level_controller_create(void)
{
    sinricpro_power_level_controller_handle_t handle =
        malloc(sizeof(struct sinricpro_power_level_controller));

    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate PowerLevelController");
        return NULL;
    }

    handle->callback = NULL;
    handle->adjust_callback = NULL;
    handle->user_data = NULL;
    handle->adjust_user_data = NULL;
    handle->limiter = sinricpro_event_limiter_create(SINRICPRO_EVENT_LIMIT_STATE);

    if (handle->limiter == NULL) {
        ESP_LOGE(TAG, "Failed to create event limiter");
        free(handle);
        return NULL;
    }

    ESP_LOGD(TAG, "PowerLevelController created");
    return handle;
}

esp_err_t sinricpro_power_level_controller_set_callback(
    sinricpro_power_level_controller_handle_t handle,
    sinricpro_power_level_callback_t callback,
    void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->callback = callback;
    handle->user_data = user_data;
    ESP_LOGD(TAG, "Power level callback registered");
    return ESP_OK;
}

esp_err_t sinricpro_power_level_controller_set_adjust_callback(
    sinricpro_power_level_controller_handle_t handle,
    sinricpro_adjust_power_level_callback_t callback,
    void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->adjust_callback = callback;
    handle->adjust_user_data = user_data;
    ESP_LOGD(TAG, "Adjust power level callback registered");
    return ESP_OK;
}

bool sinricpro_power_level_controller_handle_request(
    sinricpro_power_level_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value)
{
    if (handle == NULL || action == NULL) {
        return false;
    }

    if (strcmp(action, "setPowerLevel") == 0) {
        if (handle->callback == NULL) {
            ESP_LOGW(TAG, "No setPowerLevel callback registered");
            return false;
        }

        cJSON *level_item = cJSON_GetObjectItem(request_value, "powerLevel");
        if (level_item == NULL || !cJSON_IsNumber(level_item)) {
            ESP_LOGE(TAG, "Invalid powerLevel in request");
            return false;
        }

        int level = level_item->valueint;
        ESP_LOGI(TAG, "setPowerLevel: device=%s, level=%d", device_id, level);

        bool success = handle->callback(device_id, &level, handle->user_data);
        cJSON_AddNumberToObject(response_value, "powerLevel", level);
        return success;

    } else if (strcmp(action, "adjustPowerLevel") == 0) {
        if (handle->adjust_callback == NULL) {
            ESP_LOGW(TAG, "No adjustPowerLevel callback registered");
            return false;
        }

        cJSON *delta_item = cJSON_GetObjectItem(request_value, "powerLevelDelta");
        if (delta_item == NULL || !cJSON_IsNumber(delta_item)) {
            ESP_LOGE(TAG, "Invalid powerLevelDelta in request");
            return false;
        }

        int delta = delta_item->valueint;
        ESP_LOGI(TAG, "adjustPowerLevel: device=%s, delta=%d", device_id, delta);

        bool success = handle->adjust_callback(device_id, &delta, handle->adjust_user_data);
        cJSON_AddNumberToObject(response_value, "powerLevel", delta);
        return success;
    }

    return false;
}

esp_err_t sinricpro_power_level_controller_send_event(
    sinricpro_power_level_controller_handle_t handle,
    const char *device_id,
    int level,
    const char *cause)
{
    if (handle == NULL || device_id == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sinricpro_event_limiter_check(handle->limiter)) {
        uint32_t wait_ms = sinricpro_event_limiter_time_until_next(handle->limiter);
        ESP_LOGW(TAG, "Power level event rate limited (wait %lu ms)", wait_ms);
        return SINRICPRO_ERR_RATE_LIMITED;
    }

    ESP_LOGI(TAG, "Sending power level event: device=%s, level=%d, cause=%s",
             device_id, level, cause);

    cJSON *value = cJSON_CreateObject();
    cJSON_AddNumberToObject(value, "powerLevel", level);

    esp_err_t ret = sinricpro_core_send_event(device_id, "setPowerLevel", cause, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send power level event: %s", esp_err_to_name(ret));
    }

    return ret;
}

void sinricpro_power_level_controller_destroy(sinricpro_power_level_controller_handle_t handle)
{
    if (handle == NULL) return;
    if (handle->limiter) sinricpro_event_limiter_destroy(handle->limiter);
    free(handle);
    ESP_LOGD(TAG, "PowerLevelController destroyed");
}
