/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro_thermostat.h"
#include "../core/sinricpro_device_internal.h"
#include "../capabilities/power_state_controller.h"
#include "../capabilities/thermostat_controller.h"
#include "../capabilities/temperature_sensor.h"
#include "../capabilities/setting_controller.h"
#include "../capabilities/push_notification.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "thermostat_device";

typedef struct {
    sinricpro_device_t base;
    sinricpro_power_state_controller_handle_t power_state_controller;
    sinricpro_thermostat_controller_handle_t thermostat_controller;
    sinricpro_temperature_sensor_handle_t temperature_sensor;
    sinricpro_setting_controller_handle_t setting_controller;
} sinricpro_thermostat_device_t;

static bool thermostat_request_handler(
    const char *device_id,
    const char *action,
    const char *instance_id,
    cJSON *request_value,
    cJSON *response_value,
    void *user_data)
{
    sinricpro_thermostat_device_t *dev = (sinricpro_thermostat_device_t *)user_data;

    if (sinricpro_power_state_controller_handle_request(
            dev->power_state_controller,
            dev->base.device_id,
            action,
            request_value,
            response_value)) {
        return true;
    }

    if (sinricpro_thermostat_controller_handle_request(
            dev->thermostat_controller,
            dev->base.device_id,
            action,
            request_value,
            response_value)) {
        return true;
    }

    if (sinricpro_setting_controller_handle_request(
            dev->setting_controller,
            dev->base.device_id,
            action,
            request_value,
            response_value)) {
        return true;
    }

    ESP_LOGW(TAG, "Unhandled action: %s", action);
    return false;
}

sinricpro_device_handle_t sinricpro_thermostat_create(const char *device_id)
{
    if (device_id == NULL) {
        ESP_LOGE(TAG, "device_id is NULL");
        return NULL;
    }

    sinricpro_thermostat_device_t *dev = malloc(sizeof(sinricpro_thermostat_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "Failed to allocate thermostat device");
        return NULL;
    }

    memset(dev, 0, sizeof(sinricpro_thermostat_device_t));

    dev->power_state_controller = sinricpro_power_state_controller_create();
    dev->thermostat_controller = sinricpro_thermostat_controller_create();
    dev->temperature_sensor = sinricpro_temperature_sensor_capability_create();
    dev->setting_controller = sinricpro_setting_controller_create();

    if (dev->power_state_controller == NULL ||
        dev->thermostat_controller == NULL ||
        dev->temperature_sensor == NULL ||
        dev->setting_controller == NULL) {
        ESP_LOGE(TAG, "Failed to create capabilities");
        if (dev->power_state_controller) sinricpro_power_state_controller_destroy(dev->power_state_controller);
        if (dev->thermostat_controller) sinricpro_thermostat_controller_destroy(dev->thermostat_controller);
        if (dev->temperature_sensor) sinricpro_temperature_sensor_destroy(dev->temperature_sensor);
        if (dev->setting_controller) sinricpro_setting_controller_destroy(dev->setting_controller);
        free(dev);
        return NULL;
    }

    strncpy(dev->base.device_id, device_id, sizeof(dev->base.device_id) - 1);
    dev->base.request_handler = thermostat_request_handler;
    dev->base.device_type = SINRICPRO_DEVICE_TYPE_THERMOSTAT;
    dev->base.user_data = dev;
    dev->base.next = NULL;

    esp_err_t ret = sinricpro_core_register_device(&dev->base);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register thermostat device");
        sinricpro_power_state_controller_destroy(dev->power_state_controller);
        sinricpro_thermostat_controller_destroy(dev->thermostat_controller);
        sinricpro_temperature_sensor_destroy(dev->temperature_sensor);
        sinricpro_setting_controller_destroy(dev->setting_controller);
        free(dev);
        return NULL;
    }

    ESP_LOGI(TAG, "Thermostat device created: %s", device_id);
    return (sinricpro_device_handle_t)dev;
}

esp_err_t sinricpro_thermostat_on_power_state(
    sinricpro_device_handle_t device,
    sinricpro_thermostat_power_state_callback_t callback,
    void *user_data)
{
    if (device == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_thermostat_device_t *dev = (sinricpro_thermostat_device_t *)device;
    return sinricpro_power_state_controller_set_callback(
        dev->power_state_controller,
        callback,
        user_data
    );
}

esp_err_t sinricpro_thermostat_on_thermostat_mode(
    sinricpro_device_handle_t device,
    sinricpro_thermostat_mode_callback_t callback,
    void *user_data)
{
    if (device == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_thermostat_device_t *dev = (sinricpro_thermostat_device_t *)device;
    return sinricpro_thermostat_controller_set_mode_callback(
        dev->thermostat_controller,
        callback,
        user_data
    );
}

esp_err_t sinricpro_thermostat_on_target_temperature(
    sinricpro_device_handle_t device,
    sinricpro_thermostat_target_temperature_callback_t callback,
    void *user_data)
{
    if (device == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_thermostat_device_t *dev = (sinricpro_thermostat_device_t *)device;
    return sinricpro_thermostat_controller_set_target_temperature_callback(
        dev->thermostat_controller,
        callback,
        user_data
    );
}

esp_err_t sinricpro_thermostat_on_adjust_target_temperature(
    sinricpro_device_handle_t device,
    sinricpro_thermostat_adjust_temperature_callback_t callback,
    void *user_data)
{
    if (device == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_thermostat_device_t *dev = (sinricpro_thermostat_device_t *)device;
    return sinricpro_thermostat_controller_set_adjust_temperature_callback(
        dev->thermostat_controller,
        callback,
        user_data
    );
}

esp_err_t sinricpro_thermostat_send_power_state_event(
    sinricpro_device_handle_t device,
    bool state,
    const char *cause)
{
    if (device == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_thermostat_device_t *dev = (sinricpro_thermostat_device_t *)device;
    return sinricpro_power_state_controller_send_event(
        dev->power_state_controller,
        dev->base.device_id,
        state,
        cause
    );
}

esp_err_t sinricpro_thermostat_send_mode_event(
    sinricpro_device_handle_t device,
    sinricpro_thermostat_mode_t mode,
    const char *cause)
{
    if (device == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_thermostat_device_t *dev = (sinricpro_thermostat_device_t *)device;
    return sinricpro_thermostat_controller_send_mode_event(
        dev->thermostat_controller,
        dev->base.device_id,
        mode,
        cause
    );
}

esp_err_t sinricpro_thermostat_send_target_temperature_event(
    sinricpro_device_handle_t device,
    float temperature,
    const char *cause)
{
    if (device == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_thermostat_device_t *dev = (sinricpro_thermostat_device_t *)device;
    return sinricpro_thermostat_controller_send_target_temperature_event(
        dev->thermostat_controller,
        dev->base.device_id,
        temperature,
        cause
    );
}

esp_err_t sinricpro_thermostat_send_temperature_event(
    sinricpro_device_handle_t device,
    float temperature,
    float humidity,
    const char *cause)
{
    if (device == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_thermostat_device_t *dev = (sinricpro_thermostat_device_t *)device;
    return sinricpro_temperature_sensor_send_event(
        dev->temperature_sensor,
        dev->base.device_id,
        temperature,
        humidity,
        cause
    );
}
