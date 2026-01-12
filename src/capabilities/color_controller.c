/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "color_controller.h"
#include "../core/sinricpro_device_internal.h"
#include "../core/sinricpro_event_limiter.h"
#include "sinricpro_types.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "color_ctrl";

struct sinricpro_color_controller {
    sinricpro_color_callback_t callback;
    void *user_data;
    sinricpro_event_limiter_handle_t limiter;
};

sinricpro_color_controller_handle_t sinricpro_color_controller_create(void)
{
    sinricpro_color_controller_handle_t handle =
        malloc(sizeof(struct sinricpro_color_controller));

    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate ColorController");
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

    ESP_LOGD(TAG, "ColorController created");
    return handle;
}

esp_err_t sinricpro_color_controller_set_callback(
    sinricpro_color_controller_handle_t handle,
    sinricpro_color_callback_t callback,
    void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->callback = callback;
    handle->user_data = user_data;
    ESP_LOGD(TAG, "Color callback registered");
    return ESP_OK;
}

bool sinricpro_color_controller_handle_request(
    sinricpro_color_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value)
{
    if (handle == NULL || action == NULL) {
        return false;
    }

    if (strcmp(action, "setColor") == 0) {
        if (handle->callback == NULL) {
            ESP_LOGW(TAG, "No setColor callback registered");
            return false;
        }

        cJSON *color_obj = cJSON_GetObjectItem(request_value, "color");
        if (color_obj == NULL || !cJSON_IsObject(color_obj)) {
            ESP_LOGE(TAG, "Invalid color object in request");
            return false;
        }

        cJSON *r_item = cJSON_GetObjectItem(color_obj, "r");
        cJSON *g_item = cJSON_GetObjectItem(color_obj, "g");
        cJSON *b_item = cJSON_GetObjectItem(color_obj, "b");

        if (r_item == NULL || g_item == NULL || b_item == NULL ||
            !cJSON_IsNumber(r_item) || !cJSON_IsNumber(g_item) || !cJSON_IsNumber(b_item)) {
            ESP_LOGE(TAG, "Invalid RGB values in request");
            return false;
        }

        sinricpro_color_t color = {
            .r = (uint8_t)r_item->valueint,
            .g = (uint8_t)g_item->valueint,
            .b = (uint8_t)b_item->valueint
        };

        ESP_LOGI(TAG, "setColor: device=%s, r=%d, g=%d, b=%d",
                 device_id, color.r, color.g, color.b);

        bool success = handle->callback(device_id, &color, handle->user_data);

        cJSON *response_color = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_color, "r", color.r);
        cJSON_AddNumberToObject(response_color, "g", color.g);
        cJSON_AddNumberToObject(response_color, "b", color.b);
        cJSON_AddItemToObject(response_value, "color", response_color);

        return success;
    }

    return false;
}

esp_err_t sinricpro_color_controller_send_event(
    sinricpro_color_controller_handle_t handle,
    const char *device_id,
    const sinricpro_color_t *color,
    const char *cause)
{
    if (handle == NULL || device_id == NULL || color == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sinricpro_event_limiter_check(handle->limiter)) {
        uint32_t wait_ms = sinricpro_event_limiter_time_until_next(handle->limiter);
        ESP_LOGW(TAG, "Color event rate limited (wait %lu ms)", wait_ms);
        return SINRICPRO_ERR_RATE_LIMITED;
    }

    ESP_LOGI(TAG, "Sending color event: device=%s, r=%d, g=%d, b=%d, cause=%s",
             device_id, color->r, color->g, color->b, cause);

    cJSON *value = cJSON_CreateObject();
    cJSON *color_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(color_obj, "r", color->r);
    cJSON_AddNumberToObject(color_obj, "g", color->g);
    cJSON_AddNumberToObject(color_obj, "b", color->b);
    cJSON_AddItemToObject(value, "color", color_obj);

    esp_err_t ret = sinricpro_core_send_event(device_id, "setColor", cause, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send color event: %s", esp_err_to_name(ret));
    }

    return ret;
}

void sinricpro_color_controller_destroy(sinricpro_color_controller_handle_t handle)
{
    if (handle == NULL) return;
    if (handle->limiter) sinricpro_event_limiter_destroy(handle->limiter);
    free(handle);
    ESP_LOGD(TAG, "ColorController destroyed");
}
