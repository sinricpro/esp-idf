/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "equalizer_controller.h"
#include "../core/sinricpro_device_internal.h"
#include "../core/sinricpro_event_limiter.h"
#include "sinricpro_types.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "equalizer_ctrl";

struct sinricpro_equalizer_controller {
    sinricpro_equalizer_callback_t callback;
    void *user_data;
    sinricpro_event_limiter_handle_t limiter;
};

sinricpro_equalizer_controller_handle_t sinricpro_equalizer_controller_create(void)
{
    sinricpro_equalizer_controller_handle_t handle =
        malloc(sizeof(struct sinricpro_equalizer_controller));

    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate EqualizerController");
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

    ESP_LOGD(TAG, "EqualizerController created");
    return handle;
}

esp_err_t sinricpro_equalizer_controller_set_callback(
    sinricpro_equalizer_controller_handle_t handle,
    sinricpro_equalizer_callback_t callback,
    void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->callback = callback;
    handle->user_data = user_data;
    ESP_LOGD(TAG, "Equalizer callback registered");
    return ESP_OK;
}

bool sinricpro_equalizer_controller_handle_request(
    sinricpro_equalizer_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value)
{
    if (handle == NULL || action == NULL) {
        return false;
    }

    if (strcmp(action, "setEqualizerBands") == 0) {
        if (handle->callback == NULL) {
            ESP_LOGW(TAG, "No equalizer callback registered");
            return false;
        }

        cJSON *bands_obj = cJSON_GetObjectItem(request_value, "bands");
        if (bands_obj == NULL || !cJSON_IsArray(bands_obj)) {
            ESP_LOGE(TAG, "Invalid bands in request");
            return false;
        }

        sinricpro_equalizer_bands_t bands = {0, 0, 0};

        cJSON *band_item = NULL;
        cJSON_ArrayForEach(band_item, bands_obj) {
            cJSON *name_item = cJSON_GetObjectItem(band_item, "name");
            cJSON *level_item = cJSON_GetObjectItem(band_item, "level");

            if (name_item && cJSON_IsString(name_item) && level_item && cJSON_IsNumber(level_item)) {
                const char *name = name_item->valuestring;
                int level = level_item->valueint;

                if (strcmp(name, "BASS") == 0) bands.bass = level;
                else if (strcmp(name, "MIDRANGE") == 0) bands.midrange = level;
                else if (strcmp(name, "TREBLE") == 0) bands.treble = level;
            }
        }

        ESP_LOGI(TAG, "setEqualizerBands: device=%s, bass=%d, mid=%d, treble=%d",
                 device_id, bands.bass, bands.midrange, bands.treble);

        bool success = handle->callback(device_id, &bands, handle->user_data);

        cJSON *response_bands = cJSON_CreateArray();
        cJSON *bass_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(bass_obj, "name", "BASS");
        cJSON_AddNumberToObject(bass_obj, "level", bands.bass);
        cJSON_AddItemToArray(response_bands, bass_obj);

        cJSON *mid_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(mid_obj, "name", "MIDRANGE");
        cJSON_AddNumberToObject(mid_obj, "level", bands.midrange);
        cJSON_AddItemToArray(response_bands, mid_obj);

        cJSON *treble_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(treble_obj, "name", "TREBLE");
        cJSON_AddNumberToObject(treble_obj, "level", bands.treble);
        cJSON_AddItemToArray(response_bands, treble_obj);

        cJSON_AddItemToObject(response_value, "bands", response_bands);
        return success;
    }

    return false;
}

esp_err_t sinricpro_equalizer_controller_send_event(
    sinricpro_equalizer_controller_handle_t handle,
    const char *device_id,
    const sinricpro_equalizer_bands_t *bands,
    const char *cause)
{
    if (handle == NULL || device_id == NULL || bands == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sinricpro_event_limiter_check(handle->limiter)) {
        uint32_t wait_ms = sinricpro_event_limiter_time_until_next(handle->limiter);
        ESP_LOGW(TAG, "Equalizer event rate limited (wait %lu ms)", wait_ms);
        return SINRICPRO_ERR_RATE_LIMITED;
    }

    ESP_LOGI(TAG, "Sending equalizer event: device=%s, bass=%d, mid=%d, treble=%d, cause=%s",
             device_id, bands->bass, bands->midrange, bands->treble, cause);

    cJSON *value = cJSON_CreateObject();
    cJSON *bands_array = cJSON_CreateArray();

    cJSON *bass_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(bass_obj, "name", "BASS");
    cJSON_AddNumberToObject(bass_obj, "level", bands->bass);
    cJSON_AddItemToArray(bands_array, bass_obj);

    cJSON *mid_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(mid_obj, "name", "MIDRANGE");
    cJSON_AddNumberToObject(mid_obj, "level", bands->midrange);
    cJSON_AddItemToArray(bands_array, mid_obj);

    cJSON *treble_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(treble_obj, "name", "TREBLE");
    cJSON_AddNumberToObject(treble_obj, "level", bands->treble);
    cJSON_AddItemToArray(bands_array, treble_obj);

    cJSON_AddItemToObject(value, "bands", bands_array);

    esp_err_t ret = sinricpro_core_send_event(device_id, "setEqualizerBands", cause, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send equalizer event: %s", esp_err_to_name(ret));
    }

    return ret;
}

void sinricpro_equalizer_controller_destroy(sinricpro_equalizer_controller_handle_t handle)
{
    if (handle == NULL) return;
    if (handle->limiter) sinricpro_event_limiter_destroy(handle->limiter);
    free(handle);
    ESP_LOGD(TAG, "EqualizerController destroyed");
}
