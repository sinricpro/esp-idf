/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "thermostat_controller.h"
#include "../core/sinricpro_device_internal.h"
#include "../core/sinricpro_event_limiter.h"
#include "sinricpro_types.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"
#include <math.h>

static const char *TAG = "thermostat_ctrl";

struct sinricpro_thermostat_controller {
    sinricpro_thermostat_mode_callback_t mode_callback;
    sinricpro_target_temperature_callback_t temp_callback;
    sinricpro_adjust_target_temperature_callback_t adjust_temp_callback;
    void *mode_user_data;
    void *temp_user_data;
    void *adjust_temp_user_data;
    sinricpro_event_limiter_handle_t limiter;
};

static const char* mode_to_string(sinricpro_thermostat_mode_t mode)
{
    switch (mode) {
        case SINRICPRO_THERMOSTAT_MODE_AUTO: return "AUTO";
        case SINRICPRO_THERMOSTAT_MODE_COOL: return "COOL";
        case SINRICPRO_THERMOSTAT_MODE_HEAT: return "HEAT";
        case SINRICPRO_THERMOSTAT_MODE_ECO: return "ECO";
        case SINRICPRO_THERMOSTAT_MODE_OFF: return "OFF";
        default: return "AUTO";
    }
}

static sinricpro_thermostat_mode_t string_to_mode(const char *mode_str)
{
    if (strcmp(mode_str, "AUTO") == 0) return SINRICPRO_THERMOSTAT_MODE_AUTO;
    if (strcmp(mode_str, "COOL") == 0) return SINRICPRO_THERMOSTAT_MODE_COOL;
    if (strcmp(mode_str, "HEAT") == 0) return SINRICPRO_THERMOSTAT_MODE_HEAT;
    if (strcmp(mode_str, "ECO") == 0) return SINRICPRO_THERMOSTAT_MODE_ECO;
    if (strcmp(mode_str, "OFF") == 0) return SINRICPRO_THERMOSTAT_MODE_OFF;
    return SINRICPRO_THERMOSTAT_MODE_AUTO;
}

sinricpro_thermostat_controller_handle_t sinricpro_thermostat_controller_create(void)
{
    sinricpro_thermostat_controller_handle_t handle =
        malloc(sizeof(struct sinricpro_thermostat_controller));

    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate ThermostatController");
        return NULL;
    }

    handle->mode_callback = NULL;
    handle->temp_callback = NULL;
    handle->adjust_temp_callback = NULL;
    handle->mode_user_data = NULL;
    handle->temp_user_data = NULL;
    handle->adjust_temp_user_data = NULL;
    handle->limiter = sinricpro_event_limiter_create(SINRICPRO_EVENT_LIMIT_STATE);

    if (handle->limiter == NULL) {
        ESP_LOGE(TAG, "Failed to create event limiter");
        free(handle);
        return NULL;
    }

    ESP_LOGD(TAG, "ThermostatController created");
    return handle;
}

esp_err_t sinricpro_thermostat_controller_set_mode_callback(
    sinricpro_thermostat_controller_handle_t handle,
    sinricpro_thermostat_mode_callback_t callback,
    void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->mode_callback = callback;
    handle->mode_user_data = user_data;
    ESP_LOGD(TAG, "Thermostat mode callback registered");
    return ESP_OK;
}

esp_err_t sinricpro_thermostat_controller_set_target_temperature_callback(
    sinricpro_thermostat_controller_handle_t handle,
    sinricpro_target_temperature_callback_t callback,
    void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->temp_callback = callback;
    handle->temp_user_data = user_data;
    ESP_LOGD(TAG, "Target temperature callback registered");
    return ESP_OK;
}

esp_err_t sinricpro_thermostat_controller_set_adjust_temperature_callback(
    sinricpro_thermostat_controller_handle_t handle,
    sinricpro_adjust_target_temperature_callback_t callback,
    void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->adjust_temp_callback = callback;
    handle->adjust_temp_user_data = user_data;
    ESP_LOGD(TAG, "Adjust target temperature callback registered");
    return ESP_OK;
}

bool sinricpro_thermostat_controller_handle_request(
    sinricpro_thermostat_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value)
{
    if (handle == NULL || action == NULL) {
        return false;
    }

    if (strcmp(action, "setThermostatMode") == 0) {
        if (handle->mode_callback == NULL) {
            ESP_LOGW(TAG, "No thermostat mode callback registered");
            return false;
        }

        cJSON *mode_item = cJSON_GetObjectItem(request_value, "thermostatMode");
        if (mode_item == NULL || !cJSON_IsString(mode_item)) {
            ESP_LOGE(TAG, "Invalid thermostatMode in request");
            return false;
        }

        sinricpro_thermostat_mode_t mode = string_to_mode(mode_item->valuestring);
        ESP_LOGI(TAG, "setThermostatMode: device=%s, mode=%s", device_id, mode_item->valuestring);

        bool success = handle->mode_callback(device_id, &mode, handle->mode_user_data);
        cJSON_AddStringToObject(response_value, "thermostatMode", mode_to_string(mode));
        return success;

    } else if (strcmp(action, "targetTemperature") == 0) {
        if (handle->temp_callback == NULL) {
            ESP_LOGW(TAG, "No target temperature callback registered");
            return false;
        }

        cJSON *temp_item = cJSON_GetObjectItem(request_value, "temperature");
        if (temp_item == NULL || !cJSON_IsNumber(temp_item)) {
            ESP_LOGE(TAG, "Invalid temperature in request");
            return false;
        }

        float temperature = (float)temp_item->valuedouble;
        ESP_LOGI(TAG, "targetTemperature: device=%s, temp=%.1f°C", device_id, temperature);

        bool success = handle->temp_callback(device_id, &temperature, handle->temp_user_data);
        cJSON_AddNumberToObject(response_value, "temperature", roundf(temperature * 10.0f) / 10.0f);
        return success;

    } else if (strcmp(action, "adjustTargetTemperature") == 0) {
        if (handle->adjust_temp_callback == NULL) {
            ESP_LOGW(TAG, "No adjust target temperature callback registered");
            return false;
        }

        cJSON *delta_item = cJSON_GetObjectItem(request_value, "temperature");
        if (delta_item == NULL || !cJSON_IsNumber(delta_item)) {
            ESP_LOGE(TAG, "Invalid temperature delta in request");
            return false;
        }

        float delta = (float)delta_item->valuedouble;
        ESP_LOGI(TAG, "adjustTargetTemperature: device=%s, delta=%.1f°C", device_id, delta);

        bool success = handle->adjust_temp_callback(device_id, &delta, handle->adjust_temp_user_data);
        cJSON_AddNumberToObject(response_value, "temperature", roundf(delta * 10.0f) / 10.0f);
        return success;
    }

    return false;
}

esp_err_t sinricpro_thermostat_controller_send_mode_event(
    sinricpro_thermostat_controller_handle_t handle,
    const char *device_id,
    sinricpro_thermostat_mode_t mode,
    const char *cause)
{
    if (handle == NULL || device_id == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sinricpro_event_limiter_check(handle->limiter)) {
        uint32_t wait_ms = sinricpro_event_limiter_time_until_next(handle->limiter);
        ESP_LOGW(TAG, "Thermostat mode event rate limited (wait %lu ms)", wait_ms);
        return SINRICPRO_ERR_RATE_LIMITED;
    }

    const char *mode_str = mode_to_string(mode);
    ESP_LOGI(TAG, "Sending thermostat mode event: device=%s, mode=%s, cause=%s",
             device_id, mode_str, cause);

    cJSON *value = cJSON_CreateObject();
    cJSON_AddStringToObject(value, "thermostatMode", mode_str);

    esp_err_t ret = sinricpro_core_send_event(device_id, "setThermostatMode", cause, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send thermostat mode event: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t sinricpro_thermostat_controller_send_target_temperature_event(
    sinricpro_thermostat_controller_handle_t handle,
    const char *device_id,
    float temperature,
    const char *cause)
{
    if (handle == NULL || device_id == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sinricpro_event_limiter_check(handle->limiter)) {
        uint32_t wait_ms = sinricpro_event_limiter_time_until_next(handle->limiter);
        ESP_LOGW(TAG, "Target temperature event rate limited (wait %lu ms)", wait_ms);
        return SINRICPRO_ERR_RATE_LIMITED;
    }

    ESP_LOGI(TAG, "Sending target temperature event: device=%s, temp=%.1f°C, cause=%s",
             device_id, temperature, cause);

    cJSON *value = cJSON_CreateObject();
    cJSON_AddNumberToObject(value, "temperature", roundf(temperature * 10.0f) / 10.0f);

    esp_err_t ret = sinricpro_core_send_event(device_id, "targetTemperature", cause, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send target temperature event: %s", esp_err_to_name(ret));
    }

    return ret;
}

void sinricpro_thermostat_controller_destroy(sinricpro_thermostat_controller_handle_t handle)
{
    if (handle == NULL) return;
    if (handle->limiter) sinricpro_event_limiter_destroy(handle->limiter);
    free(handle);
    ESP_LOGD(TAG, "ThermostatController destroyed");
}
