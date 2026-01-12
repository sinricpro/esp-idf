/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro_fan.h"
#include "../core/sinricpro_device_internal.h"
#include "../capabilities/power_state_controller.h"
#include "../capabilities/power_level_controller.h"
#include "../capabilities/setting_controller.h"
#include "../capabilities/push_notification.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "sinricpro_fan";

typedef struct {
    sinricpro_device_t base;
    sinricpro_power_state_controller_handle_t power_state_controller;
    sinricpro_power_level_controller_handle_t power_level_controller;
    sinricpro_setting_controller_handle_t setting_controller;
    sinricpro_push_notification_handle_t push_notification;
} sinricpro_fan_device_t;

static bool fan_request_handler(
    const char *device_id,
    const char *action,
    const char *instance_id,
    cJSON *request_value,
    cJSON *response_value,
    void *user_data)
{
    sinricpro_fan_device_t *dev = (sinricpro_fan_device_t *)user_data;

    /* Try power state controller */
    if (sinricpro_power_state_controller_handle_request(dev->power_state_controller,
                                                          device_id, action,
                                                          request_value, response_value)) {
        return true;
    }

    /* Try power level controller (fan speed) */
    if (sinricpro_power_level_controller_handle_request(dev->power_level_controller,
                                                          device_id, action,
                                                          request_value, response_value)) {
        return true;
    }

    /* Try setting controller */
    if (sinricpro_setting_controller_handle_request(dev->setting_controller,
                                                      device_id, action,
                                                      instance_id, request_value,
                                                      response_value)) {
        return true;
    }

    ESP_LOGW(TAG, "Unhandled action: %s", action);
    return false;
}

sinricpro_device_handle_t sinricpro_fan_create(const char *device_id)
{
    if (device_id == NULL) {
        ESP_LOGE(TAG, "device_id is NULL");
        return NULL;
    }

    sinricpro_fan_device_t *dev = malloc(sizeof(sinricpro_fan_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "Failed to allocate Fan device");
        return NULL;
    }

    memset(dev, 0, sizeof(sinricpro_fan_device_t));

    strncpy(dev->base.device_id, device_id, sizeof(dev->base.device_id) - 1);
    dev->base.device_type = SINRICPRO_DEVICE_TYPE_FAN;
    dev->base.request_handler = fan_request_handler;
    dev->base.user_data = dev;

    dev->power_state_controller = sinricpro_power_state_controller_create();
    dev->power_level_controller = sinricpro_power_level_controller_create();
    dev->setting_controller = sinricpro_setting_controller_create();
    dev->push_notification = sinricpro_push_notification_create();

    if (dev->power_state_controller == NULL || dev->power_level_controller == NULL ||
        dev->setting_controller == NULL || dev->push_notification == NULL) {
        ESP_LOGE(TAG, "Failed to create capabilities");
        sinricpro_fan_delete((sinricpro_device_handle_t)dev);
        return NULL;
    }

    esp_err_t ret = sinricpro_core_register_device(&dev->base);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register device: %s", esp_err_to_name(ret));
        sinricpro_fan_delete((sinricpro_device_handle_t)dev);
        return NULL;
    }

    ESP_LOGI(TAG, "Fan device created: %s", device_id);
    return (sinricpro_device_handle_t)dev;
}

esp_err_t sinricpro_fan_delete(sinricpro_device_handle_t device)
{
    if (device == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_fan_device_t *dev = (sinricpro_fan_device_t *)device;

    sinricpro_core_unregister_device(dev->base.device_id);

    if (dev->power_state_controller) sinricpro_power_state_controller_destroy(dev->power_state_controller);
    if (dev->power_level_controller) sinricpro_power_level_controller_destroy(dev->power_level_controller);
    if (dev->setting_controller) sinricpro_setting_controller_destroy(dev->setting_controller);
    if (dev->push_notification) sinricpro_push_notification_destroy(dev->push_notification);

    free(dev);
    ESP_LOGI(TAG, "Fan device deleted");
    return ESP_OK;
}

esp_err_t sinricpro_fan_on_power_state(
    sinricpro_device_handle_t device,
    sinricpro_power_state_callback_t callback,
    void *user_data)
{
    if (device == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_fan_device_t *dev = (sinricpro_fan_device_t *)device;
    return sinricpro_power_state_controller_set_callback(dev->power_state_controller, callback, user_data);
}

esp_err_t sinricpro_fan_on_power_level(
    sinricpro_device_handle_t device,
    sinricpro_power_level_callback_t callback,
    void *user_data)
{
    if (device == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_fan_device_t *dev = (sinricpro_fan_device_t *)device;
    return sinricpro_power_level_controller_set_callback(dev->power_level_controller, callback, user_data);
}

esp_err_t sinricpro_fan_on_adjust_power_level(
    sinricpro_device_handle_t device,
    sinricpro_adjust_power_level_callback_t callback,
    void *user_data)
{
    if (device == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_fan_device_t *dev = (sinricpro_fan_device_t *)device;
    return sinricpro_power_level_controller_set_adjust_callback(dev->power_level_controller, callback, user_data);
}

esp_err_t sinricpro_fan_send_power_state_event(
    sinricpro_device_handle_t device,
    bool state,
    const char *cause)
{
    if (device == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_fan_device_t *dev = (sinricpro_fan_device_t *)device;
    return sinricpro_power_state_controller_send_event(dev->power_state_controller,
                                                         dev->base.device_id,
                                                         state,
                                                         cause);
}

esp_err_t sinricpro_fan_send_power_level_event(
    sinricpro_device_handle_t device,
    int level,
    const char *cause)
{
    if (device == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_fan_device_t *dev = (sinricpro_fan_device_t *)device;
    return sinricpro_power_level_controller_send_event(dev->power_level_controller,
                                                         dev->base.device_id,
                                                         level,
                                                         cause);
}
