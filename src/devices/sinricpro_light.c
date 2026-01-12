/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro_light.h"
#include "../core/sinricpro_device_internal.h"
#include "../capabilities/power_state_controller.h"
#include "../capabilities/brightness_controller.h"
#include "../capabilities/color_controller.h"
#include "../capabilities/color_temperature_controller.h"
#include "../capabilities/setting_controller.h"
#include "../capabilities/push_notification.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "light_device";

typedef struct {
    sinricpro_device_t base;
    sinricpro_power_state_controller_handle_t power_state_controller;
    sinricpro_brightness_controller_handle_t brightness_controller;
    sinricpro_color_controller_handle_t color_controller;
    sinricpro_color_temperature_controller_handle_t color_temperature_controller;
    sinricpro_setting_controller_handle_t setting_controller;
    sinricpro_push_notification_handle_t push_notification;
} sinricpro_light_device_t;

static bool light_request_handler(
    sinricpro_device_handle_t device,
    const char *action,
    cJSON *request_value,
    cJSON *response_value)
{
    sinricpro_light_device_t *dev = (sinricpro_light_device_t *)device;

    if (sinricpro_power_state_controller_handle_request(
            dev->power_state_controller,
            dev->base.device_id,
            action,
            request_value,
            response_value)) {
        return true;
    }

    if (sinricpro_brightness_controller_handle_request(
            dev->brightness_controller,
            dev->base.device_id,
            action,
            request_value,
            response_value)) {
        return true;
    }

    if (sinricpro_color_controller_handle_request(
            dev->color_controller,
            dev->base.device_id,
            action,
            request_value,
            response_value)) {
        return true;
    }

    if (sinricpro_color_temperature_controller_handle_request(
            dev->color_temperature_controller,
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

sinricpro_device_handle_t sinricpro_light_create(const char *device_id)
{
    if (device_id == NULL) {
        ESP_LOGE(TAG, "device_id is NULL");
        return NULL;
    }

    sinricpro_light_device_t *dev = malloc(sizeof(sinricpro_light_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "Failed to allocate light device");
        return NULL;
    }

    memset(dev, 0, sizeof(sinricpro_light_device_t));

    dev->power_state_controller = sinricpro_power_state_controller_create();
    dev->brightness_controller = sinricpro_brightness_controller_create();
    dev->color_controller = sinricpro_color_controller_create();
    dev->color_temperature_controller = sinricpro_color_temperature_controller_create();
    dev->setting_controller = sinricpro_setting_controller_create();
    dev->push_notification = sinricpro_push_notification_create();

    if (dev->power_state_controller == NULL ||
        dev->brightness_controller == NULL ||
        dev->color_controller == NULL ||
        dev->color_temperature_controller == NULL ||
        dev->setting_controller == NULL ||
        dev->push_notification == NULL) {
        ESP_LOGE(TAG, "Failed to create capabilities");
        if (dev->power_state_controller) sinricpro_power_state_controller_destroy(dev->power_state_controller);
        if (dev->brightness_controller) sinricpro_brightness_controller_destroy(dev->brightness_controller);
        if (dev->color_controller) sinricpro_color_controller_destroy(dev->color_controller);
        if (dev->color_temperature_controller) sinricpro_color_temperature_controller_destroy(dev->color_temperature_controller);
        if (dev->setting_controller) sinricpro_setting_controller_destroy(dev->setting_controller);
        if (dev->push_notification) sinricpro_push_notification_destroy(dev->push_notification);
        free(dev);
        return NULL;
    }

    dev->base.device_id = device_id;
    dev->base.request_handler = light_request_handler;

    if (sinricpro_device_register((sinricpro_device_handle_t)dev) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register light device");
        sinricpro_power_state_controller_destroy(dev->power_state_controller);
        sinricpro_brightness_controller_destroy(dev->brightness_controller);
        sinricpro_color_controller_destroy(dev->color_controller);
        sinricpro_color_temperature_controller_destroy(dev->color_temperature_controller);
        sinricpro_setting_controller_destroy(dev->setting_controller);
        sinricpro_push_notification_destroy(dev->push_notification);
        free(dev);
        return NULL;
    }

    ESP_LOGI(TAG, "Light device created: %s", device_id);
    return (sinricpro_device_handle_t)dev;
}

esp_err_t sinricpro_light_on_power_state(
    sinricpro_device_handle_t device,
    sinricpro_light_power_state_callback_t callback,
    void *user_data)
{
    if (device == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_light_device_t *dev = (sinricpro_light_device_t *)device;
    return sinricpro_power_state_controller_set_callback(
        dev->power_state_controller,
        callback,
        user_data
    );
}

esp_err_t sinricpro_light_on_brightness(
    sinricpro_device_handle_t device,
    sinricpro_light_brightness_callback_t callback,
    void *user_data)
{
    if (device == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_light_device_t *dev = (sinricpro_light_device_t *)device;
    return sinricpro_brightness_controller_set_callback(
        dev->brightness_controller,
        callback,
        user_data
    );
}

esp_err_t sinricpro_light_on_adjust_brightness(
    sinricpro_device_handle_t device,
    sinricpro_light_adjust_brightness_callback_t callback,
    void *user_data)
{
    if (device == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_light_device_t *dev = (sinricpro_light_device_t *)device;
    return sinricpro_brightness_controller_set_adjust_callback(
        dev->brightness_controller,
        callback,
        user_data
    );
}

esp_err_t sinricpro_light_on_color(
    sinricpro_device_handle_t device,
    sinricpro_light_color_callback_t callback,
    void *user_data)
{
    if (device == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_light_device_t *dev = (sinricpro_light_device_t *)device;
    return sinricpro_color_controller_set_callback(
        dev->color_controller,
        (sinricpro_color_callback_t)callback,
        user_data
    );
}

esp_err_t sinricpro_light_on_color_temperature(
    sinricpro_device_handle_t device,
    sinricpro_light_color_temperature_callback_t callback,
    void *user_data)
{
    if (device == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_light_device_t *dev = (sinricpro_light_device_t *)device;
    return sinricpro_color_temperature_controller_set_callback(
        dev->color_temperature_controller,
        callback,
        user_data
    );
}

esp_err_t sinricpro_light_on_adjust_color_temperature(
    sinricpro_device_handle_t device,
    sinricpro_light_adjust_color_temperature_callback_t callback,
    void *user_data)
{
    if (device == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_light_device_t *dev = (sinricpro_light_device_t *)device;
    return sinricpro_color_temperature_controller_set_adjust_callback(
        dev->color_temperature_controller,
        callback,
        user_data
    );
}

esp_err_t sinricpro_light_send_power_state_event(
    sinricpro_device_handle_t device,
    bool state,
    const char *cause)
{
    if (device == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_light_device_t *dev = (sinricpro_light_device_t *)device;
    return sinricpro_power_state_controller_send_event(
        dev->power_state_controller,
        dev->base.device_id,
        state,
        cause
    );
}

esp_err_t sinricpro_light_send_brightness_event(
    sinricpro_device_handle_t device,
    int brightness,
    const char *cause)
{
    if (device == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_light_device_t *dev = (sinricpro_light_device_t *)device;
    return sinricpro_brightness_controller_send_event(
        dev->brightness_controller,
        dev->base.device_id,
        brightness,
        cause
    );
}

esp_err_t sinricpro_light_send_color_event(
    sinricpro_device_handle_t device,
    const sinricpro_light_color_t *color,
    const char *cause)
{
    if (device == NULL || color == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_light_device_t *dev = (sinricpro_light_device_t *)device;
    return sinricpro_color_controller_send_event(
        dev->color_controller,
        dev->base.device_id,
        (const sinricpro_color_t *)color,
        cause
    );
}

esp_err_t sinricpro_light_send_color_temperature_event(
    sinricpro_device_handle_t device,
    int color_temperature,
    const char *cause)
{
    if (device == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_light_device_t *dev = (sinricpro_light_device_t *)device;
    return sinricpro_color_temperature_controller_send_event(
        dev->color_temperature_controller,
        dev->base.device_id,
        color_temperature,
        cause
    );
}
