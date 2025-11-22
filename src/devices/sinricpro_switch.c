/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro_switch.h"
#include "../core/sinricpro_device_internal.h"
#include "../capabilities/power_state_controller.h"
#include "../capabilities/setting_controller.h"
#include "../capabilities/push_notification.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "sinricpro_switch";

/**
 * @brief Switch device structure
 */
typedef struct {
    sinricpro_device_t base;  /* Base device structure */
    sinricpro_power_state_controller_handle_t power_state;
    sinricpro_setting_controller_handle_t setting;
} sinricpro_switch_device_t;

/**
 * @brief Request handler for switch device
 */
static bool switch_request_handler(const char *device_id,
                                     const char *action,
                                     const char *instance_id,
                                     cJSON *request_value,
                                     cJSON *response_value,
                                     void *user_data)
{
    sinricpro_switch_device_t *device = (sinricpro_switch_device_t *)user_data;

    ESP_LOGD(TAG, "Switch request: device=%s, action=%s", device_id, action);

    /* Try PowerStateController */
    if (sinricpro_power_state_controller_handle_request(device->power_state,
                                                          device_id,
                                                          action,
                                                          request_value,
                                                          response_value)) {
        return true;
    }

    /* Try SettingController */
    if (sinricpro_setting_controller_handle_request(device->setting,
                                                      device_id,
                                                      action,
                                                      request_value,
                                                      response_value)) {
        return true;
    }

    ESP_LOGW(TAG, "Unhandled action: %s", action);
    return false;
}

sinricpro_device_handle_t sinricpro_switch_create(const char *device_id)
{
    if (device_id == NULL) {
        ESP_LOGE(TAG, "Invalid device_id");
        return NULL;
    }

    /* Validate device_id length */
    size_t id_len = strlen(device_id);
    if (id_len == 0 || id_len >= CONFIG_SINRICPRO_MAX_DEVICE_ID_LEN) {
        ESP_LOGE(TAG, "Invalid device_id length: %zu", id_len);
        return NULL;
    }

    /* Allocate switch device */
    sinricpro_switch_device_t *device = malloc(sizeof(sinricpro_switch_device_t));
    if (device == NULL) {
        ESP_LOGE(TAG, "Failed to allocate switch device");
        return NULL;
    }

    memset(device, 0, sizeof(sinricpro_switch_device_t));

    /* Initialize base device */
    strncpy(device->base.device_id, device_id, sizeof(device->base.device_id) - 1);
    device->base.device_type = SINRICPRO_DEVICE_TYPE_SWITCH;
    device->base.request_handler = switch_request_handler;
    device->base.user_data = device;
    device->base.next = NULL;

    /* Create capabilities */
    device->power_state = sinricpro_power_state_controller_create();
    if (device->power_state == NULL) {
        ESP_LOGE(TAG, "Failed to create PowerStateController");
        free(device);
        return NULL;
    }

    device->setting = sinricpro_setting_controller_create();
    if (device->setting == NULL) {
        ESP_LOGE(TAG, "Failed to create SettingController");
        sinricpro_power_state_controller_destroy(device->power_state);
        free(device);
        return NULL;
    }

    /* Register device with core */
    esp_err_t ret = sinricpro_core_register_device(&device->base);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register device: %s", esp_err_to_name(ret));
        sinricpro_setting_controller_destroy(device->setting);
        sinricpro_power_state_controller_destroy(device->power_state);
        free(device);
        return NULL;
    }

    ESP_LOGI(TAG, "Switch device created: %s", device_id);

    return (sinricpro_device_handle_t)device;
}

esp_err_t sinricpro_switch_on_power_state(sinricpro_device_handle_t handle,
                                           sinricpro_power_state_callback_t callback,
                                           void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_switch_device_t *device = (sinricpro_switch_device_t *)handle;

    return sinricpro_power_state_controller_set_callback(device->power_state,
                                                           callback,
                                                           user_data);
}

esp_err_t sinricpro_switch_on_setting(sinricpro_device_handle_t handle,
                                       sinricpro_setting_callback_t callback,
                                       void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_switch_device_t *device = (sinricpro_switch_device_t *)handle;

    return sinricpro_setting_controller_set_callback(device->setting,
                                                       callback,
                                                       user_data);
}

esp_err_t sinricpro_switch_send_power_state_event(sinricpro_device_handle_t handle,
                                                    bool state,
                                                    const char *cause)
{
    if (handle == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_switch_device_t *device = (sinricpro_switch_device_t *)handle;

    return sinricpro_power_state_controller_send_event(device->power_state,
                                                         device->base.device_id,
                                                         state,
                                                         cause);
}

esp_err_t sinricpro_switch_send_notification(sinricpro_device_handle_t handle,
                                              const char *message)
{
    if (handle == NULL || message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_switch_device_t *device = (sinricpro_switch_device_t *)handle;

    return sinricpro_send_push_notification(device->base.device_id, message);
}

esp_err_t sinricpro_switch_delete(sinricpro_device_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_switch_device_t *device = (sinricpro_switch_device_t *)handle;

    /* Unregister from core */
    esp_err_t ret = sinricpro_core_unregister_device(device->base.device_id);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to unregister device: %s", esp_err_to_name(ret));
    }

    /* Destroy capabilities */
    if (device->setting) {
        sinricpro_setting_controller_destroy(device->setting);
    }

    if (device->power_state) {
        sinricpro_power_state_controller_destroy(device->power_state);
    }

    /* Free device */
    free(device);

    ESP_LOGI(TAG, "Switch device deleted");

    return ESP_OK;
}
