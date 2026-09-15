/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro_camera.h"
#include "../core/sinricpro_device_internal.h"
#include "../capabilities/power_state_controller.h"
#include "../capabilities/camera_controller.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "sinricpro_camera";

/**
 * @brief Camera device structure
 */
typedef struct {
    sinricpro_device_t base;  /* Base device structure */
    sinricpro_power_state_controller_handle_t power_state;
    sinricpro_camera_controller_handle_t camera;
} sinricpro_camera_device_t;

/**
 * @brief Request handler for camera device
 */
static bool camera_request_handler(const char *device_id,
                                   const char *action,
                                   const char *instance_id,
                                   cJSON *request_value,
                                   cJSON *response_value,
                                   void *user_data)
{
    sinricpro_camera_device_t *device = (sinricpro_camera_device_t *)user_data;

    ESP_LOGD(TAG, "Camera request: device=%s, action=%s", device_id, action);

    if (sinricpro_camera_controller_owns_action(action)) {
        return sinricpro_camera_controller_handle_request(device->camera,
                                                          device_id,
                                                          action,
                                                          request_value,
                                                          response_value);
    }

    if (sinricpro_power_state_controller_handle_request(device->power_state,
                                                         device_id,
                                                         action,
                                                         request_value,
                                                         response_value)) {
        return true;
    }

    ESP_LOGW(TAG, "Unhandled action: %s", action);
    return false;
}

sinricpro_device_handle_t sinricpro_camera_create(const char *device_id)
{
    if (device_id == NULL) {
        ESP_LOGE(TAG, "Invalid device_id");
        return NULL;
    }

    size_t id_len = strlen(device_id);
    if (id_len == 0 || id_len >= CONFIG_SINRICPRO_MAX_DEVICE_ID_LEN) {
        ESP_LOGE(TAG, "Invalid device_id length: %zu", id_len);
        return NULL;
    }

    sinricpro_camera_device_t *device = calloc(1, sizeof(sinricpro_camera_device_t));
    if (device == NULL) {
        ESP_LOGE(TAG, "Failed to allocate camera device");
        return NULL;
    }

    strncpy(device->base.device_id, device_id, sizeof(device->base.device_id) - 1);
    device->base.device_type = SINRICPRO_DEVICE_TYPE_CAMERA;
    device->base.request_handler = camera_request_handler;
    device->base.user_data = device;
    device->base.next = NULL;

    device->power_state = sinricpro_power_state_controller_create();
    device->camera = sinricpro_camera_controller_create();
    if (device->power_state == NULL || device->camera == NULL) {
        ESP_LOGE(TAG, "Failed to create camera capabilities");
        sinricpro_camera_controller_destroy(device->camera);
        if (device->power_state) {
            sinricpro_power_state_controller_destroy(device->power_state);
        }
        free(device);
        return NULL;
    }

    esp_err_t ret = sinricpro_core_register_device(&device->base);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register device: %s", esp_err_to_name(ret));
        sinricpro_camera_controller_destroy(device->camera);
        sinricpro_power_state_controller_destroy(device->power_state);
        free(device);
        return NULL;
    }

    ESP_LOGI(TAG, "Camera device created: %s", device_id);

    return (sinricpro_device_handle_t)device;
}

esp_err_t sinricpro_camera_on_power_state(sinricpro_device_handle_t handle,
                                          sinricpro_camera_power_state_callback_t callback,
                                          void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_camera_device_t *device = (sinricpro_camera_device_t *)handle;

    return sinricpro_power_state_controller_set_callback(device->power_state,
                                                          callback,
                                                          user_data);
}

esp_err_t sinricpro_camera_on_webrtc_offer(sinricpro_device_handle_t handle,
                                           sinricpro_camera_webrtc_offer_callback_t callback,
                                           void *user_data)
{
    if (handle == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_camera_device_t *device = (sinricpro_camera_device_t *)handle;

    return sinricpro_camera_controller_set_webrtc_offer_callback(device->camera,
                                                                  callback,
                                                                  user_data);
}

esp_err_t sinricpro_camera_enable_webrtc_audio(sinricpro_device_handle_t handle, bool enabled)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_camera_device_t *device = (sinricpro_camera_device_t *)handle;
    sinricpro_camera_controller_set_webrtc_audio(device->camera, enabled);
    return ESP_OK;
}

esp_err_t sinricpro_camera_send_power_state_event(sinricpro_device_handle_t handle,
                                                  bool state,
                                                  const char *cause)
{
    if (handle == NULL || cause == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_camera_device_t *device = (sinricpro_camera_device_t *)handle;

    return sinricpro_power_state_controller_send_event(device->power_state,
                                                        device->base.device_id,
                                                        state,
                                                        cause);
}

esp_err_t sinricpro_camera_delete(sinricpro_device_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sinricpro_camera_device_t *device = (sinricpro_camera_device_t *)handle;

    esp_err_t ret = sinricpro_core_unregister_device(device->base.device_id);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to unregister device: %s", esp_err_to_name(ret));
    }

    sinricpro_camera_controller_destroy(device->camera);
    sinricpro_power_state_controller_destroy(device->power_state);
    free(device);

    ESP_LOGI(TAG, "Camera device deleted");

    return ESP_OK;
}
