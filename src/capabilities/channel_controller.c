/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "channel_controller.h"
#include "../core/sinricpro_device_internal.h"
#include "../core/sinricpro_event_limiter.h"
#include "sinricpro_types.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "channel_ctrl";

struct sinricpro_channel_controller {
    sinricpro_channel_callback_t callback;
    sinricpro_skip_channels_callback_t skip_callback;
    void *user_data;
    void *skip_user_data;
    sinricpro_event_limiter_handle_t limiter;
};

sinricpro_channel_controller_handle_t sinricpro_channel_controller_create(void)
{
    sinricpro_channel_controller_handle_t handle =
        malloc(sizeof(struct sinricpro_channel_controller));

    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate ChannelController");
        return NULL;
    }

    handle->callback = NULL;
    handle->skip_callback = NULL;
    handle->user_data = NULL;
    handle->skip_user_data = NULL;
    handle->limiter = sinricpro_event_limiter_create(SINRICPRO_EVENT_LIMIT_STATE);

    if (handle->limiter == NULL) {
        ESP_LOGE(TAG, "Failed to create event limiter");
        free(handle);
        return NULL;
    }

    ESP_LOGD(TAG, "ChannelController created");
    return handle;
}

esp_err_t sinricpro_channel_controller_set_callback(
    sinricpro_channel_controller_handle_t handle,
    sinricpro_channel_callback_t callback,
    void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->callback = callback;
    handle->user_data = user_data;
    ESP_LOGD(TAG, "Channel callback registered");
    return ESP_OK;
}

esp_err_t sinricpro_channel_controller_set_skip_callback(
    sinricpro_channel_controller_handle_t handle,
    sinricpro_skip_channels_callback_t callback,
    void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->skip_callback = callback;
    handle->skip_user_data = user_data;
    ESP_LOGD(TAG, "Skip channels callback registered");
    return ESP_OK;
}

bool sinricpro_channel_controller_handle_request(
    sinricpro_channel_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value)
{
    if (handle == NULL || action == NULL) {
        return false;
    }

    if (strcmp(action, "changeChannel") == 0) {
        if (handle->callback == NULL) {
            ESP_LOGW(TAG, "No channel callback registered");
            return false;
        }

        cJSON *channel_obj = cJSON_GetObjectItem(request_value, "channel");
        if (channel_obj == NULL || !cJSON_IsObject(channel_obj)) {
            ESP_LOGE(TAG, "Invalid channel in request");
            return false;
        }

        cJSON *number_item = cJSON_GetObjectItem(channel_obj, "number");
        cJSON *name_item = cJSON_GetObjectItem(channel_obj, "name");

        sinricpro_channel_t channel = {
            .number = (number_item && cJSON_IsNumber(number_item)) ? number_item->valueint : 0,
            .name = (name_item && cJSON_IsString(name_item)) ? name_item->valuestring : NULL
        };

        ESP_LOGI(TAG, "changeChannel: device=%s, number=%d, name=%s",
                 device_id, channel.number, channel.name ? channel.name : "");

        bool success = handle->callback(device_id, &channel, handle->user_data);

        cJSON *response_channel = cJSON_CreateObject();
        cJSON_AddNumberToObject(response_channel, "number", channel.number);
        if (channel.name) {
            cJSON_AddStringToObject(response_channel, "name", channel.name);
        }
        cJSON_AddItemToObject(response_value, "channel", response_channel);
        return success;

    } else if (strcmp(action, "skipChannels") == 0) {
        if (handle->skip_callback == NULL) {
            ESP_LOGW(TAG, "No skip channels callback registered");
            return false;
        }

        cJSON *count_item = cJSON_GetObjectItem(request_value, "channelCount");
        if (count_item == NULL || !cJSON_IsNumber(count_item)) {
            ESP_LOGE(TAG, "Invalid channelCount in request");
            return false;
        }

        int count = count_item->valueint;
        ESP_LOGI(TAG, "skipChannels: device=%s, count=%d", device_id, count);

        bool success = handle->skip_callback(device_id, count, handle->skip_user_data);
        cJSON_AddNumberToObject(response_value, "channelCount", count);
        return success;
    }

    return false;
}

esp_err_t sinricpro_channel_controller_send_event(
    sinricpro_channel_controller_handle_t handle,
    const char *device_id,
    const sinricpro_channel_t *channel,
    const char *cause)
{
    if (handle == NULL || device_id == NULL || channel == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sinricpro_event_limiter_check(handle->limiter)) {
        uint32_t wait_ms = sinricpro_event_limiter_time_until_next(handle->limiter);
        ESP_LOGW(TAG, "Channel event rate limited (wait %lu ms)", wait_ms);
        return SINRICPRO_ERR_RATE_LIMITED;
    }

    ESP_LOGI(TAG, "Sending channel event: device=%s, number=%d, name=%s, cause=%s",
             device_id, channel->number, channel->name ? channel->name : "", cause);

    cJSON *value = cJSON_CreateObject();
    cJSON *channel_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(channel_obj, "number", channel->number);
    if (channel->name) {
        cJSON_AddStringToObject(channel_obj, "name", channel->name);
    }
    cJSON_AddItemToObject(value, "channel", channel_obj);

    esp_err_t ret = sinricpro_core_send_event(device_id, "changeChannel", cause, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send channel event: %s", esp_err_to_name(ret));
    }

    return ret;
}

void sinricpro_channel_controller_destroy(sinricpro_channel_controller_handle_t handle)
{
    if (handle == NULL) return;
    if (handle->limiter) sinricpro_event_limiter_destroy(handle->limiter);
    free(handle);
    ESP_LOGD(TAG, "ChannelController destroyed");
}
