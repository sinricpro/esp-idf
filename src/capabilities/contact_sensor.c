/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "contact_sensor.h"
#include "../core/sinricpro_device_internal.h"
#include "../core/sinricpro_event_limiter.h"
#include "sinricpro_types.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "contact_sensor";

/**
 * @brief ContactSensor context
 */
struct sinricpro_contact_sensor {
    sinricpro_event_limiter_handle_t limiter;
};

sinricpro_contact_sensor_handle_t sinricpro_contact_sensor_capability_create(void)
{
    sinricpro_contact_sensor_handle_t handle =
        malloc(sizeof(struct sinricpro_contact_sensor));

    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate ContactSensor");
        return NULL;
    }

    handle->limiter = sinricpro_event_limiter_create(SINRICPRO_EVENT_LIMIT_SENSOR);

    if (handle->limiter == NULL) {
        ESP_LOGE(TAG, "Failed to create event limiter");
        free(handle);
        return NULL;
    }

    ESP_LOGD(TAG, "ContactSensor created");

    return handle;
}

esp_err_t sinricpro_contact_sensor_send_event(
    sinricpro_contact_sensor_handle_t handle,
    const char *device_id,
    bool detected,
    const char *cause)
{
    if (handle == NULL || device_id == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Check rate limit */
    if (!sinricpro_event_limiter_check(handle->limiter)) {
        uint32_t wait_ms = sinricpro_event_limiter_time_until_next(handle->limiter);
        ESP_LOGW(TAG, "Contact event rate limited (wait %lu ms)", wait_ms);
        return SINRICPRO_ERR_RATE_LIMITED;
    }

    ESP_LOGI(TAG, "Sending contact event: device=%s, detected=%s, cause=%s",
             device_id, detected ? "closed" : "open", cause);

    /* Create value object */
    cJSON *value = cJSON_CreateObject();
    cJSON_AddStringToObject(value, "state", detected ? "closed" : "open");

    /* Send event */
    esp_err_t ret = sinricpro_core_send_event(device_id, "setContactState", cause, value);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send contact event: %s", esp_err_to_name(ret));
    }

    return ret;
}

void sinricpro_contact_sensor_destroy(sinricpro_contact_sensor_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

    if (handle->limiter) {
        sinricpro_event_limiter_destroy(handle->limiter);
    }

    free(handle);

    ESP_LOGD(TAG, "ContactSensor destroyed");
}
