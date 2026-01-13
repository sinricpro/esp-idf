/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "power_sensor.h"
#include "../core/sinricpro_device_internal.h"
#include "../core/sinricpro_event_limiter.h"
#include "sinricpro_types.h"
#include "sinricpro.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "power_sensor";

struct sinricpro_power_sensor {
    sinricpro_event_limiter_handle_t limiter;
    uint32_t start_time;
    float last_power;
};

sinricpro_power_sensor_handle_t sinricpro_power_sensor_capability_create(void)
{
    sinricpro_power_sensor_handle_t handle =
        malloc(sizeof(struct sinricpro_power_sensor));

    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate PowerSensor");
        return NULL;
    }

    handle->limiter = sinricpro_event_limiter_create(SINRICPRO_EVENT_LIMIT_SENSOR);
    handle->start_time = 0;
    handle->last_power = 0.0f;

    if (handle->limiter == NULL) {
        ESP_LOGE(TAG, "Failed to create event limiter");
        free(handle);
        return NULL;
    }

    ESP_LOGD(TAG, "PowerSensor created");
    return handle;
}

esp_err_t sinricpro_power_sensor_send_event(
    sinricpro_power_sensor_handle_t handle,
    const char *device_id,
    float voltage,
    float current,
    float power,
    float apparent_power,
    float reactive_power,
    float factor,
    const char *cause)
{
    if (handle == NULL || device_id == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sinricpro_event_limiter_check(handle->limiter)) {
        uint32_t wait_ms = sinricpro_event_limiter_time_until_next(handle->limiter);
        ESP_LOGW(TAG, "Power sensor event rate limited (wait %lu ms)", wait_ms);
        return SINRICPRO_ERR_RATE_LIMITED;
    }

    /* Calculate power if not provided */
    if (power < 0.0f) {
        power = voltage * current;
    }

    /* Calculate factor if not provided but apparent_power is */
    if (factor < 0.0f && apparent_power >= 0.0f) {
        factor = (apparent_power > 0.0f) ? (power / apparent_power) : 0.0f;
    }

    uint32_t current_time = sinricpro_get_timestamp();
    float watt_hours = 0.0f;

    /* Calculate watt-hours if we have previous data */
    if (handle->start_time > 0) {
        uint32_t elapsed = current_time - handle->start_time;
        watt_hours = (elapsed * handle->last_power) / 3600.0f;
    }

    ESP_LOGI(TAG, "Sending power sensor event: device=%s, V=%.1f, A=%.2f, W=%.1f",
             device_id, voltage, current, power);

    cJSON *value = cJSON_CreateObject();
    cJSON_AddNumberToObject(value, "startTime", handle->start_time);
    cJSON_AddNumberToObject(value, "voltage", voltage);
    cJSON_AddNumberToObject(value, "current", current);
    cJSON_AddNumberToObject(value, "power", power);
    cJSON_AddNumberToObject(value, "apparentPower", apparent_power);
    cJSON_AddNumberToObject(value, "reactivePower", reactive_power);
    cJSON_AddNumberToObject(value, "factor", factor);
    cJSON_AddNumberToObject(value, "wattHours", watt_hours);

    /* Update tracking state */
    handle->start_time = current_time;
    handle->last_power = power;

    esp_err_t ret = sinricpro_core_send_event(device_id, "powerUsage", cause, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send power sensor event: %s", esp_err_to_name(ret));
    }

    return ret;
}

void sinricpro_power_sensor_destroy(sinricpro_power_sensor_handle_t handle)
{
    if (handle == NULL) return;
    if (handle->limiter) sinricpro_event_limiter_destroy(handle->limiter);
    free(handle);
    ESP_LOGD(TAG, "PowerSensor destroyed");
}
