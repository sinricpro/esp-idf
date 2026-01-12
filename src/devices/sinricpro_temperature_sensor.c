/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro_temperature_sensor.h"
#include "../core/sinricpro_device_internal.h"
#include "../capabilities/temperature_sensor.h"
#include "../capabilities/setting_controller.h"
#include "../capabilities/push_notification.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "sinricpro_temp_sensor";

typedef struct {
    sinricpro_device_t base;
    sinricpro_temperature_sensor_handle_t temperature_sensor;
    sinricpro_setting_controller_handle_t setting_controller;
    sinricpro_push_notification_handle_t push_notification;
} sinricpro_temperature_sensor_device_t;

static bool temperature_sensor_request_handler(
    const char *device_id,
    const char *action,
    const char *instance_id,
    cJSON *request_value,
    cJSON *response_value,
    void *user_data)
{
    sinricpro_temperature_sensor_device_t *dev = (sinricpro_temperature_sensor_device_t *)user_data;

    if (sinricpro_setting_controller_handle_request(dev->setting_controller,
                                                      device_id, action,
                                                      instance_id, request_value,
                                                      response_value)) {
        return true;
    }

    ESP_LOGW(TAG, "Unhandled action: %s", action);
    return false;
}

sinricpro_device_handle_t sinricpro_temperature_sensor_create(const char *device_id)
{
    if (device_id == NULL) {
        ESP_LOGE(TAG, "device_id is NULL");
        return NULL;
    }

    sinricpro_temperature_sensor_device_t *dev = malloc(sizeof(sinricpro_temperature_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "Failed to allocate TemperatureSensor device");
        return NULL;
    }

    memset(dev, 0, sizeof(sinricpro_temperature_sensor_device_t));

    strncpy(dev->base.device_id, device_id, sizeof(dev->base.device_id) - 1);
    dev->base.device_type = SINRICPRO_DEVICE_TYPE_TEMPERATURE_SENSOR;
    dev->base.request_handler = temperature_sensor_request_handler;
    dev->base.user_data = dev;

    dev->temperature_sensor = sinricpro_temperature_sensor_create();
    dev->setting_controller = sinricpro_setting_controller_create();
    dev->push_notification = sinricpro_push_notification_create();

    if (dev->temperature_sensor == NULL || dev->setting_controller == NULL ||
        dev->push_notification == NULL) {
        ESP_LOGE(TAG, "Failed to create capabilities");
        sinricpro_temperature_sensor_delete((sinricpro_device_handle_t)dev);
        return NULL;
    }

    esp_err_t ret = sinricpro_core_register_device(&dev->base);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register device: %s", esp_err_to_name(ret));
        sinricpro_temperature_sensor_delete((sinricpro_device_handle_t)dev);
        return NULL;
    }

    ESP_LOGI(TAG, "TemperatureSensor device created: %s", device_id);
    return (sinricpro_device_handle_t)dev;
}

esp_err_t sinricpro_temperature_sensor_delete(sinricpro_device_handle_t device)
{
    if (device == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_temperature_sensor_device_t *dev = (sinricpro_temperature_sensor_device_t *)device;

    sinricpro_core_unregister_device(dev->base.device_id);

    if (dev->temperature_sensor) sinricpro_temperature_sensor_destroy(dev->temperature_sensor);
    if (dev->setting_controller) sinricpro_setting_controller_destroy(dev->setting_controller);
    if (dev->push_notification) sinricpro_push_notification_destroy(dev->push_notification);

    free(dev);
    ESP_LOGI(TAG, "TemperatureSensor device deleted");
    return ESP_OK;
}

esp_err_t sinricpro_temperature_sensor_send_temperature_event(
    sinricpro_device_handle_t device,
    float temperature,
    float humidity,
    const char *cause)
{
    if (device == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_temperature_sensor_device_t *dev = (sinricpro_temperature_sensor_device_t *)device;
    return sinricpro_temperature_sensor_send_event(dev->temperature_sensor,
                                                     dev->base.device_id,
                                                     temperature,
                                                     humidity,
                                                     cause);
}
