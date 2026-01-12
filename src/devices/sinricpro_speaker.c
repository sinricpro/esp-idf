/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro_speaker.h"
#include "../core/sinricpro_device_internal.h"
#include "../capabilities/power_state_controller.h"
#include "../capabilities/volume_controller.h"
#include "../capabilities/mute_controller.h"
#include "../capabilities/media_controller.h"
#include "../capabilities/input_controller.h"
#include "../capabilities/equalizer_controller.h"
#include "../capabilities/mode_controller.h"
#include "../capabilities/setting_controller.h"
#include "../capabilities/push_notification.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "speaker_device";

typedef struct {
    sinricpro_device_t base;
    sinricpro_power_state_controller_handle_t power_state_controller;
    sinricpro_volume_controller_handle_t volume_controller;
    sinricpro_mute_controller_handle_t mute_controller;
    sinricpro_media_controller_handle_t media_controller;
    sinricpro_input_controller_handle_t input_controller;
    sinricpro_equalizer_controller_handle_t equalizer_controller;
    sinricpro_mode_controller_handle_t mode_controller;
    sinricpro_setting_controller_handle_t setting_controller;
    sinricpro_push_notification_handle_t push_notification;
} sinricpro_speaker_device_t;

static bool speaker_request_handler(
    sinricpro_device_handle_t device,
    const char *action,
    cJSON *request_value,
    cJSON *response_value)
{
    sinricpro_speaker_device_t *dev = (sinricpro_speaker_device_t *)device;

    if (sinricpro_power_state_controller_handle_request(
            dev->power_state_controller, dev->base.device_id, action, request_value, response_value)) {
        return true;
    }

    if (sinricpro_volume_controller_handle_request(
            dev->volume_controller, dev->base.device_id, action, request_value, response_value)) {
        return true;
    }

    if (sinricpro_mute_controller_handle_request(
            dev->mute_controller, dev->base.device_id, action, request_value, response_value)) {
        return true;
    }

    if (sinricpro_media_controller_handle_request(
            dev->media_controller, dev->base.device_id, action, request_value, response_value)) {
        return true;
    }

    if (sinricpro_input_controller_handle_request(
            dev->input_controller, dev->base.device_id, action, request_value, response_value)) {
        return true;
    }

    if (sinricpro_equalizer_controller_handle_request(
            dev->equalizer_controller, dev->base.device_id, action, request_value, response_value)) {
        return true;
    }

    if (sinricpro_mode_controller_handle_request(
            dev->mode_controller, dev->base.device_id, action, request_value, response_value)) {
        return true;
    }

    if (sinricpro_setting_controller_handle_request(
            dev->setting_controller, dev->base.device_id, action, request_value, response_value)) {
        return true;
    }

    ESP_LOGW(TAG, "Unhandled action: %s", action);
    return false;
}

sinricpro_device_handle_t sinricpro_speaker_create(const char *device_id)
{
    if (device_id == NULL) {
        ESP_LOGE(TAG, "device_id is NULL");
        return NULL;
    }

    sinricpro_speaker_device_t *dev = malloc(sizeof(sinricpro_speaker_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "Failed to allocate Speaker device");
        return NULL;
    }

    memset(dev, 0, sizeof(sinricpro_speaker_device_t));

    dev->power_state_controller = sinricpro_power_state_controller_create();
    dev->volume_controller = sinricpro_volume_controller_create();
    dev->mute_controller = sinricpro_mute_controller_create();
    dev->media_controller = sinricpro_media_controller_create();
    dev->input_controller = sinricpro_input_controller_create();
    dev->equalizer_controller = sinricpro_equalizer_controller_create();
    dev->mode_controller = sinricpro_mode_controller_create();
    dev->setting_controller = sinricpro_setting_controller_create();
    dev->push_notification = sinricpro_push_notification_create();

    if (!dev->power_state_controller || !dev->volume_controller || !dev->mute_controller ||
        !dev->media_controller || !dev->input_controller || !dev->equalizer_controller ||
        !dev->mode_controller || !dev->setting_controller || !dev->push_notification) {
        ESP_LOGE(TAG, "Failed to create capabilities");
        if (dev->power_state_controller) sinricpro_power_state_controller_destroy(dev->power_state_controller);
        if (dev->volume_controller) sinricpro_volume_controller_destroy(dev->volume_controller);
        if (dev->mute_controller) sinricpro_mute_controller_destroy(dev->mute_controller);
        if (dev->media_controller) sinricpro_media_controller_destroy(dev->media_controller);
        if (dev->input_controller) sinricpro_input_controller_destroy(dev->input_controller);
        if (dev->equalizer_controller) sinricpro_equalizer_controller_destroy(dev->equalizer_controller);
        if (dev->mode_controller) sinricpro_mode_controller_destroy(dev->mode_controller);
        if (dev->setting_controller) sinricpro_setting_controller_destroy(dev->setting_controller);
        if (dev->push_notification) sinricpro_push_notification_destroy(dev->push_notification);
        free(dev);
        return NULL;
    }

    dev->base.device_id = device_id;
    dev->base.request_handler = speaker_request_handler;

    if (sinricpro_device_register((sinricpro_device_handle_t)dev) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register Speaker device");
        sinricpro_power_state_controller_destroy(dev->power_state_controller);
        sinricpro_volume_controller_destroy(dev->volume_controller);
        sinricpro_mute_controller_destroy(dev->mute_controller);
        sinricpro_media_controller_destroy(dev->media_controller);
        sinricpro_input_controller_destroy(dev->input_controller);
        sinricpro_equalizer_controller_destroy(dev->equalizer_controller);
        sinricpro_mode_controller_destroy(dev->mode_controller);
        sinricpro_setting_controller_destroy(dev->setting_controller);
        sinricpro_push_notification_destroy(dev->push_notification);
        free(dev);
        return NULL;
    }

    ESP_LOGI(TAG, "Speaker device created: %s", device_id);
    return (sinricpro_device_handle_t)dev;
}

esp_err_t sinricpro_speaker_on_power_state(sinricpro_device_handle_t device, sinricpro_speaker_power_state_callback_t callback, void *user_data) {
    if (device == NULL || callback == NULL) return ESP_ERR_INVALID_ARG;
    sinricpro_speaker_device_t *dev = (sinricpro_speaker_device_t *)device;
    return sinricpro_power_state_controller_set_callback(dev->power_state_controller, callback, user_data);
}

esp_err_t sinricpro_speaker_on_volume(sinricpro_device_handle_t device, sinricpro_speaker_volume_callback_t callback, void *user_data) {
    if (device == NULL || callback == NULL) return ESP_ERR_INVALID_ARG;
    sinricpro_speaker_device_t *dev = (sinricpro_speaker_device_t *)device;
    return sinricpro_volume_controller_set_callback(dev->volume_controller, callback, user_data);
}

esp_err_t sinricpro_speaker_on_adjust_volume(sinricpro_device_handle_t device, sinricpro_speaker_adjust_volume_callback_t callback, void *user_data) {
    if (device == NULL || callback == NULL) return ESP_ERR_INVALID_ARG;
    sinricpro_speaker_device_t *dev = (sinricpro_speaker_device_t *)device;
    return sinricpro_volume_controller_set_adjust_callback(dev->volume_controller, callback, user_data);
}

esp_err_t sinricpro_speaker_on_mute(sinricpro_device_handle_t device, sinricpro_speaker_mute_callback_t callback, void *user_data) {
    if (device == NULL || callback == NULL) return ESP_ERR_INVALID_ARG;
    sinricpro_speaker_device_t *dev = (sinricpro_speaker_device_t *)device;
    return sinricpro_mute_controller_set_callback(dev->mute_controller, callback, user_data);
}

esp_err_t sinricpro_speaker_on_media_control(sinricpro_device_handle_t device, sinricpro_speaker_media_control_callback_t callback, void *user_data) {
    if (device == NULL || callback == NULL) return ESP_ERR_INVALID_ARG;
    sinricpro_speaker_device_t *dev = (sinricpro_speaker_device_t *)device;
    return sinricpro_media_controller_set_callback(dev->media_controller, callback, user_data);
}

esp_err_t sinricpro_speaker_on_input(sinricpro_device_handle_t device, sinricpro_speaker_input_callback_t callback, void *user_data) {
    if (device == NULL || callback == NULL) return ESP_ERR_INVALID_ARG;
    sinricpro_speaker_device_t *dev = (sinricpro_speaker_device_t *)device;
    return sinricpro_input_controller_set_callback(dev->input_controller, callback, user_data);
}

esp_err_t sinricpro_speaker_on_equalizer(sinricpro_device_handle_t device, sinricpro_speaker_equalizer_callback_t callback, void *user_data) {
    if (device == NULL || callback == NULL) return ESP_ERR_INVALID_ARG;
    sinricpro_speaker_device_t *dev = (sinricpro_speaker_device_t *)device;
    return sinricpro_equalizer_controller_set_callback(dev->equalizer_controller, (sinricpro_equalizer_callback_t)callback, user_data);
}

esp_err_t sinricpro_speaker_on_mode(sinricpro_device_handle_t device, sinricpro_speaker_mode_callback_t callback, void *user_data) {
    if (device == NULL || callback == NULL) return ESP_ERR_INVALID_ARG;
    sinricpro_speaker_device_t *dev = (sinricpro_speaker_device_t *)device;
    return sinricpro_mode_controller_set_callback(dev->mode_controller, callback, user_data);
}

esp_err_t sinricpro_speaker_send_power_state_event(sinricpro_device_handle_t device, bool state, const char *cause) {
    if (device == NULL || cause == NULL) return ESP_ERR_INVALID_ARG;
    sinricpro_speaker_device_t *dev = (sinricpro_speaker_device_t *)device;
    return sinricpro_power_state_controller_send_event(dev->power_state_controller, dev->base.device_id, state, cause);
}

esp_err_t sinricpro_speaker_send_volume_event(sinricpro_device_handle_t device, int volume, const char *cause) {
    if (device == NULL || cause == NULL) return ESP_ERR_INVALID_ARG;
    sinricpro_speaker_device_t *dev = (sinricpro_speaker_device_t *)device;
    return sinricpro_volume_controller_send_event(dev->volume_controller, dev->base.device_id, volume, cause);
}

esp_err_t sinricpro_speaker_send_mute_event(sinricpro_device_handle_t device, bool mute, const char *cause) {
    if (device == NULL || cause == NULL) return ESP_ERR_INVALID_ARG;
    sinricpro_speaker_device_t *dev = (sinricpro_speaker_device_t *)device;
    return sinricpro_mute_controller_send_event(dev->mute_controller, dev->base.device_id, mute, cause);
}

esp_err_t sinricpro_speaker_send_media_control_event(sinricpro_device_handle_t device, const char *control, const char *cause) {
    if (device == NULL || control == NULL || cause == NULL) return ESP_ERR_INVALID_ARG;
    sinricpro_speaker_device_t *dev = (sinricpro_speaker_device_t *)device;
    return sinricpro_media_controller_send_event(dev->media_controller, dev->base.device_id, control, cause);
}

esp_err_t sinricpro_speaker_send_input_event(sinricpro_device_handle_t device, const char *input, const char *cause) {
    if (device == NULL || input == NULL || cause == NULL) return ESP_ERR_INVALID_ARG;
    sinricpro_speaker_device_t *dev = (sinricpro_speaker_device_t *)device;
    return sinricpro_input_controller_send_event(dev->input_controller, dev->base.device_id, input, cause);
}

esp_err_t sinricpro_speaker_send_equalizer_event(sinricpro_device_handle_t device, const sinricpro_speaker_equalizer_bands_t *bands, const char *cause) {
    if (device == NULL || bands == NULL || cause == NULL) return ESP_ERR_INVALID_ARG;
    sinricpro_speaker_device_t *dev = (sinricpro_speaker_device_t *)device;
    return sinricpro_equalizer_controller_send_event(dev->equalizer_controller, dev->base.device_id, (const sinricpro_equalizer_bands_t *)bands, cause);
}

esp_err_t sinricpro_speaker_send_mode_event(sinricpro_device_handle_t device, const char *mode, const char *cause) {
    if (device == NULL || mode == NULL || cause == NULL) return ESP_ERR_INVALID_ARG;
    sinricpro_speaker_device_t *dev = (sinricpro_speaker_device_t *)device;
    return sinricpro_mode_controller_send_event(dev->mode_controller, dev->base.device_id, mode, cause);
}
