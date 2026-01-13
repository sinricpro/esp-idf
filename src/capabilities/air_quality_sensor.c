/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "air_quality_sensor.h"
#include "../core/sinricpro_device_internal.h"
#include "../core/sinricpro_event_limiter.h"
#include "sinricpro_types.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "airquality_sensor";

struct sinricpro_air_quality_sensor {
    sinricpro_event_limiter_handle_t limiter;
};

sinricpro_air_quality_sensor_handle_t sinricpro_air_quality_sensor_capability_create(void)
{
    sinricpro_air_quality_sensor_handle_t handle =
        malloc(sizeof(struct sinricpro_air_quality_sensor));

    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate AirQualitySensor");
        return NULL;
    }

    handle->limiter = sinricpro_event_limiter_create(SINRICPRO_EVENT_LIMIT_SENSOR);
    if (handle->limiter == NULL) {
        ESP_LOGE(TAG, "Failed to create event limiter");
        free(handle);
        return NULL;
    }

    ESP_LOGD(TAG, "AirQualitySensor created");
    return handle;
}

esp_err_t sinricpro_air_quality_sensor_send_event(
    sinricpro_air_quality_sensor_handle_t handle,
    const char *device_id,
    int pm1,
    int pm2_5,
    int pm10,
    const char *cause)
{
    if (handle == NULL || device_id == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sinricpro_event_limiter_check(handle->limiter)) {
        uint32_t wait_ms = sinricpro_event_limiter_time_until_next(handle->limiter);
        ESP_LOGW(TAG, "Air quality event rate limited (wait %lu ms)", wait_ms);
        return SINRICPRO_ERR_RATE_LIMITED;
    }

    ESP_LOGI(TAG, "Sending air quality event: device=%s, PM1=%d, PM2.5=%d, PM10=%d",
             device_id, pm1, pm2_5, pm10);

    cJSON *value = cJSON_CreateObject();
    cJSON_AddNumberToObject(value, "pm1", pm1);
    cJSON_AddNumberToObject(value, "pm2_5", pm2_5);
    cJSON_AddNumberToObject(value, "pm10", pm10);

    esp_err_t ret = sinricpro_core_send_event(device_id, "airQuality", cause, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send air quality event: %s", esp_err_to_name(ret));
    }

    return ret;
}

void sinricpro_air_quality_sensor_destroy(sinricpro_air_quality_sensor_handle_t handle)
{
    if (handle == NULL) return;
    if (handle->limiter) sinricpro_event_limiter_destroy(handle->limiter);
    free(handle);
    ESP_LOGD(TAG, "AirQualitySensor destroyed");
}
