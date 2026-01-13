/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro_garage_door.h"
#include "../core/sinricpro_device_internal.h"
#include "../capabilities/door_controller.h"
#include "../capabilities/setting_controller.h"
#include "../capabilities/push_notification.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "sinricpro_garage_door";

typedef struct {
    sinricpro_device_t base;
    sinricpro_door_controller_handle_t door_controller;
    sinricpro_setting_controller_handle_t setting_controller;
} sinricpro_garage_door_device_t;

static bool garage_door_request_handler(
    const char *device_id,
    const char *action,
    const char *instance_id,
    cJSON *request_value,
    cJSON *response_value,
    void *user_data)
{
    sinricpro_garage_door_device_t *dev = (sinricpro_garage_door_device_t *)user_data;

    /* Try door controller */
    if (sinricpro_door_controller_handle_request(dev->door_controller,
                                                   device_id, action,
                                                   request_value, response_value)) {
        return true;
    }

    /* Try setting controller */
    if (sinricpro_setting_controller_handle_request(dev->setting_controller,
                                                      device_id, action,
                                                      request_value,
                                                      response_value)) {
        return true;
    }

    ESP_LOGW(TAG, "Unhandled action: %s", action);
    return false;
}

sinricpro_device_handle_t sinricpro_garage_door_create(const char *device_id)
{
    if (device_id == NULL) {
        ESP_LOGE(TAG, "device_id is NULL");
        return NULL;
    }

    sinricpro_garage_door_device_t *dev = malloc(sizeof(sinricpro_garage_door_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "Failed to allocate GarageDoor device");
        return NULL;
    }

    memset(dev, 0, sizeof(sinricpro_garage_door_device_t));

    strncpy(dev->base.device_id, device_id, sizeof(dev->base.device_id) - 1);
    dev->base.device_type = SINRICPRO_DEVICE_TYPE_GARAGE_DOOR;
    dev->base.request_handler = garage_door_request_handler;
    dev->base.user_data = dev;

    dev->door_controller = sinricpro_door_controller_create();
    dev->setting_controller = sinricpro_setting_controller_create();

    if (dev->door_controller == NULL || dev->setting_controller == NULL) {
        ESP_LOGE(TAG, "Failed to create capabilities");
        sinricpro_garage_door_delete((sinricpro_device_handle_t)dev);
        return NULL;
    }

    esp_err_t ret = sinricpro_core_register_device(&dev->base);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register device: %s", esp_err_to_name(ret));
        sinricpro_garage_door_delete((sinricpro_device_handle_t)dev);
        return NULL;
    }

    ESP_LOGI(TAG, "GarageDoor device created: %s", device_id);
    return (sinricpro_device_handle_t)dev;
}

esp_err_t sinricpro_garage_door_delete(sinricpro_device_handle_t device)
{
    if (device == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_garage_door_device_t *dev = (sinricpro_garage_door_device_t *)device;

    sinricpro_core_unregister_device(dev->base.device_id);

    if (dev->door_controller) sinricpro_door_controller_destroy(dev->door_controller);
    if (dev->setting_controller) sinricpro_setting_controller_destroy(dev->setting_controller);

    free(dev);
    ESP_LOGI(TAG, "GarageDoor device deleted");
    return ESP_OK;
}

esp_err_t sinricpro_garage_door_on_door_state(
    sinricpro_device_handle_t device,
    sinricpro_door_state_callback_t callback,
    void *user_data)
{
    if (device == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_garage_door_device_t *dev = (sinricpro_garage_door_device_t *)device;
    return sinricpro_door_controller_set_callback(dev->door_controller, callback, user_data);
}

esp_err_t sinricpro_garage_door_send_door_state_event(
    sinricpro_device_handle_t device,
    bool state,
    const char *cause)
{
    if (device == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_garage_door_device_t *dev = (sinricpro_garage_door_device_t *)device;
    return sinricpro_door_controller_send_event(dev->door_controller,
                                                  dev->base.device_id,
                                                  state,
                                                  cause);
}
