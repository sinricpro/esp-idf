/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "input_controller.h"
#include "../core/sinricpro_device_internal.h"
#include "../core/sinricpro_event_limiter.h"
#include "sinricpro_types.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "input_ctrl";

struct sinricpro_input_controller {
    sinricpro_input_callback_t callback;
    void *user_data;
    sinricpro_event_limiter_handle_t limiter;
};

sinricpro_input_controller_handle_t sinricpro_input_controller_create(void)
{
    sinricpro_input_controller_handle_t handle =
        malloc(sizeof(struct sinricpro_input_controller));

    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate InputController");
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

    ESP_LOGD(TAG, "InputController created");
    return handle;
}

esp_err_t sinricpro_input_controller_set_callback(
    sinricpro_input_controller_handle_t handle,
    sinricpro_input_callback_t callback,
    void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->callback = callback;
    handle->user_data = user_data;
    ESP_LOGD(TAG, "Input callback registered");
    return ESP_OK;
}

bool sinricpro_input_controller_handle_request(
    sinricpro_input_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value)
{
    if (handle == NULL || action == NULL) {
        return false;
    }

    if (strcmp(action, "selectInput") == 0) {
        if (handle->callback == NULL) {
            ESP_LOGW(TAG, "No input callback registered");
            return false;
        }

        cJSON *input_item = cJSON_GetObjectItem(request_value, "input");
        if (input_item == NULL || !cJSON_IsString(input_item)) {
            ESP_LOGE(TAG, "Invalid input in request");
            return false;
        }

        const char *input = input_item->valuestring;
        ESP_LOGI(TAG, "selectInput: device=%s, input=%s", device_id, input);

        bool success = handle->callback(device_id, &input, handle->user_data);
        cJSON_AddStringToObject(response_value, "input", input);
        return success;
    }

    return false;
}

esp_err_t sinricpro_input_controller_send_event(
    sinricpro_input_controller_handle_t handle,
    const char *device_id,
    const char *input,
    const char *cause)
{
    if (handle == NULL || device_id == NULL || input == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sinricpro_event_limiter_check(handle->limiter)) {
        uint32_t wait_ms = sinricpro_event_limiter_time_until_next(handle->limiter);
        ESP_LOGW(TAG, "Input event rate limited (wait %lu ms)", wait_ms);
        return SINRICPRO_ERR_RATE_LIMITED;
    }

    ESP_LOGI(TAG, "Sending input event: device=%s, input=%s, cause=%s",
             device_id, input, cause);

    cJSON *value = cJSON_CreateObject();
    cJSON_AddStringToObject(value, "input", input);

    esp_err_t ret = sinricpro_core_send_event(device_id, "selectInput", cause, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send input event: %s", esp_err_to_name(ret));
    }

    return ret;
}

void sinricpro_input_controller_destroy(sinricpro_input_controller_handle_t handle)
{
    if (handle == NULL) return;
    if (handle->limiter) sinricpro_event_limiter_destroy(handle->limiter);
    free(handle);
    ESP_LOGD(TAG, "InputController destroyed");
}
