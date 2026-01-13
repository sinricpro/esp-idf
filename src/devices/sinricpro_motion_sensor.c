/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro_motion_sensor.h"
#include "../core/sinricpro_device_internal.h"
#include "../capabilities/motion_sensor.h"
#include "../capabilities/setting_controller.h"
#include "../capabilities/push_notification.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "sinricpro_motion_sensor";

/**
 * @brief MotionSensor device context
 */
typedef struct {
    sinricpro_device_t base;
    sinricpro_motion_sensor_handle_t motion_sensor;
    sinricpro_setting_controller_handle_t setting_controller;
} sinricpro_motion_sensor_device_t;

/**
 * @brief Request handler for MotionSensor device
 */
static bool motion_sensor_request_handler(
    const char *device_id,
    const char *action,
    const char *instance_id,
    cJSON *request_value,
    cJSON *response_value,
    void *user_data)
{
    sinricpro_motion_sensor_device_t *dev = (sinricpro_motion_sensor_device_t *)user_data;

    /* Try setting controller */
    if (sinricpro_setting_controller_handle_request(dev->setting_controller,
                                                      device_id, action,
                                                      request_value,
                                                      response_value)) {
        return true;
    }

    /* MotionSensor has no controllable actions */
    ESP_LOGW(TAG, "Unhandled action: %s", action);
    return false;
}

sinricpro_device_handle_t sinricpro_motion_sensor_create(const char *device_id)
{
    if (device_id == NULL) {
        ESP_LOGE(TAG, "device_id is NULL");
        return NULL;
    }

    /* Allocate device */
    sinricpro_motion_sensor_device_t *dev = malloc(sizeof(sinricpro_motion_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "Failed to allocate MotionSensor device");
        return NULL;
    }

    memset(dev, 0, sizeof(sinricpro_motion_sensor_device_t));

    /* Initialize base device */
    strncpy(dev->base.device_id, device_id, sizeof(dev->base.device_id) - 1);
    dev->base.device_type = SINRICPRO_DEVICE_TYPE_MOTION_SENSOR;
    dev->base.request_handler = motion_sensor_request_handler;
    dev->base.user_data = dev;

    /* Create capabilities */
    dev->motion_sensor = sinricpro_motion_sensor_capability_create();
    dev->setting_controller = sinricpro_setting_controller_create();

    if (dev->motion_sensor == NULL || dev->setting_controller == NULL) {
        ESP_LOGE(TAG, "Failed to create capabilities");
        sinricpro_motion_sensor_delete((sinricpro_device_handle_t)dev);
        return NULL;
    }

    /* Register device */
    esp_err_t ret = sinricpro_core_register_device(&dev->base);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register device: %s", esp_err_to_name(ret));
        sinricpro_motion_sensor_delete((sinricpro_device_handle_t)dev);
        return NULL;
    }

    ESP_LOGI(TAG, "MotionSensor device created: %s", device_id);

    return (sinricpro_device_handle_t)dev;
}

esp_err_t sinricpro_motion_sensor_delete(sinricpro_device_handle_t device)
{
    if (device == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_motion_sensor_device_t *dev = (sinricpro_motion_sensor_device_t *)device;

    /* Unregister device */
    sinricpro_core_unregister_device(dev->base.device_id);

    /* Destroy capabilities */
    if (dev->motion_sensor) {
        sinricpro_motion_sensor_destroy(dev->motion_sensor);
    }
    if (dev->setting_controller) {
        sinricpro_setting_controller_destroy(dev->setting_controller);
    }

    free(dev);

    ESP_LOGI(TAG, "MotionSensor device deleted");

    return ESP_OK;
}

esp_err_t sinricpro_motion_sensor_send_motion_event(
    sinricpro_device_handle_t device,
    bool detected,
    const char *cause)
{
    if (device == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_motion_sensor_device_t *dev = (sinricpro_motion_sensor_device_t *)device;

    return sinricpro_motion_sensor_send_event(dev->motion_sensor,
                                               dev->base.device_id,
                                               detected,
                                               cause);
}
