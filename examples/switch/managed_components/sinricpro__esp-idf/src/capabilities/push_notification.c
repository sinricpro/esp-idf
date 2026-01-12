/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "push_notification.h"
#include "../core/sinricpro_device_internal.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "push_notification";

esp_err_t sinricpro_send_push_notification(const char *device_id, const char *message)
{
    if (device_id == NULL || message == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Sending push notification: device=%s, message=%s", device_id, message);

    /* Create value object */
    cJSON *value = cJSON_CreateObject();
    cJSON_AddStringToObject(value, "notification", message);

    /* Send event (value is consumed by send_event) */
    esp_err_t ret = sinricpro_core_send_event(device_id, "pushNotification",
                                                SINRICPRO_CAUSE_PHYSICAL_INTERACTION,
                                                value);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send push notification: %s", esp_err_to_name(ret));
    }

    return ret;
}
