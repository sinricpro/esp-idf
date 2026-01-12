/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "color_temperature_controller.h"
#include "../core/sinricpro_device_internal.h"
#include "../core/sinricpro_event_limiter.h"
#include "sinricpro_types.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "color_temp_ctrl";

struct sinricpro_color_temperature_controller {
    sinricpro_color_temperature_callback_t callback;
    sinricpro_adjust_color_temperature_callback_t adjust_callback;
    void *user_data;
    void *adjust_user_data;
    sinricpro_event_limiter_handle_t limiter;
};

sinricpro_color_temperature_controller_handle_t sinricpro_color_temperature_controller_create(void)
{
    sinricpro_color_temperature_controller_handle_t handle =
        malloc(sizeof(struct sinricpro_color_temperature_controller));

    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate ColorTemperatureController");
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

    ESP_LOGD(TAG, "ColorTemperatureController created");
    return handle;
}

esp_err_t sinricpro_color_temperature_controller_set_callback(
    sinricpro_color_temperature_controller_handle_t handle,
    sinricpro_color_temperature_callback_t callback,
    void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->callback = callback;
    handle->user_data = user_data;
    ESP_LOGD(TAG, "Color temperature callback registered");
    return ESP_OK;
}

esp_err_t sinricpro_color_temperature_controller_set_adjust_callback(
    sinricpro_color_temperature_controller_handle_t handle,
    sinricpro_adjust_color_temperature_callback_t callback,
    void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->adjust_callback = callback;
    handle->adjust_user_data = user_data;
    ESP_LOGD(TAG, "Adjust color temperature callback registered");
    return ESP_OK;
}

bool sinricpro_color_temperature_controller_handle_request(
    sinricpro_color_temperature_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value)
{
    if (handle == NULL || action == NULL) {
        return false;
    }

    if (strcmp(action, "setColorTemperature") == 0) {
        if (handle->callback == NULL) {
            ESP_LOGW(TAG, "No setColorTemperature callback registered");
            return false;
        }

        cJSON *temp_item = cJSON_GetObjectItem(request_value, "colorTemperature");
        if (temp_item == NULL || !cJSON_IsNumber(temp_item)) {
            ESP_LOGE(TAG, "Invalid colorTemperature in request");
            return false;
        }

        int color_temperature = temp_item->valueint;
        ESP_LOGI(TAG, "setColorTemperature: device=%s, value=%dK", device_id, color_temperature);

        bool success = handle->callback(device_id, &color_temperature, handle->user_data);
        cJSON_AddNumberToObject(response_value, "colorTemperature", color_temperature);
        return success;

    } else if (strcmp(action, "increaseColorTemperature") == 0 ||
               strcmp(action, "decreaseColorTemperature") == 0) {
        if (handle->adjust_callback == NULL) {
            ESP_LOGW(TAG, "No adjust color temperature callback registered");
            return false;
        }

        int delta = (strcmp(action, "increaseColorTemperature") == 0) ? 500 : -500;
        ESP_LOGI(TAG, "%s: device=%s, delta=%d", action, device_id, delta);

        bool success = handle->adjust_callback(device_id, &delta, handle->adjust_user_data);
        cJSON_AddNumberToObject(response_value, "colorTemperature", delta);
        return success;
    }

    return false;
}

esp_err_t sinricpro_color_temperature_controller_send_event(
    sinricpro_color_temperature_controller_handle_t handle,
    const char *device_id,
    int color_temperature,
    const char *cause)
{
    if (handle == NULL || device_id == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sinricpro_event_limiter_check(handle->limiter)) {
        uint32_t wait_ms = sinricpro_event_limiter_time_until_next(handle->limiter);
        ESP_LOGW(TAG, "Color temperature event rate limited (wait %lu ms)", wait_ms);
        return SINRICPRO_ERR_RATE_LIMITED;
    }

    ESP_LOGI(TAG, "Sending color temperature event: device=%s, value=%dK, cause=%s",
             device_id, color_temperature, cause);

    cJSON *value = cJSON_CreateObject();
    cJSON_AddNumberToObject(value, "colorTemperature", color_temperature);

    esp_err_t ret = sinricpro_core_send_event(device_id, "setColorTemperature", cause, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send color temperature event: %s", esp_err_to_name(ret));
    }

    return ret;
}

void sinricpro_color_temperature_controller_destroy(sinricpro_color_temperature_controller_handle_t handle)
{
    if (handle == NULL) return;
    if (handle->limiter) sinricpro_event_limiter_destroy(handle->limiter);
    free(handle);
    ESP_LOGD(TAG, "ColorTemperatureController destroyed");
}
