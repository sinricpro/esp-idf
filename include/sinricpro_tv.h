/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_TV_H
#define SINRICPRO_TV_H

#include "sinricpro_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Channel structure
 */
typedef struct {
    int number;           /**< Channel number */
    const char *name;     /**< Channel name (optional) */
} sinricpro_tv_channel_t;

/**
 * @brief Power state callback for TV
 */
typedef bool (*sinricpro_tv_power_state_callback_t)(
    const char *device_id,
    bool *state,
    void *user_data
);

/**
 * @brief Volume callback for TV
 */
typedef bool (*sinricpro_tv_volume_callback_t)(
    const char *device_id,
    int *volume,
    void *user_data
);

/**
 * @brief Adjust volume callback for TV
 */
typedef bool (*sinricpro_tv_adjust_volume_callback_t)(
    const char *device_id,
    int *delta,
    void *user_data
);

/**
 * @brief Mute callback for TV
 */
typedef bool (*sinricpro_tv_mute_callback_t)(
    const char *device_id,
    bool *mute,
    void *user_data
);

/**
 * @brief Media control callback for TV
 */
typedef bool (*sinricpro_tv_media_control_callback_t)(
    const char *device_id,
    const char *control,
    void *user_data
);

/**
 * @brief Input callback for TV
 */
typedef bool (*sinricpro_tv_input_callback_t)(
    const char *device_id,
    const char **input,
    void *user_data
);

/**
 * @brief Channel callback for TV
 */
typedef bool (*sinricpro_tv_channel_callback_t)(
    const char *device_id,
    sinricpro_tv_channel_t *channel,
    void *user_data
);

/**
 * @brief Skip channels callback for TV
 */
typedef bool (*sinricpro_tv_skip_channels_callback_t)(
    const char *device_id,
    int count,
    void *user_data
);

/**
 * @brief Create a new TV device
 */
sinricpro_device_handle_t sinricpro_tv_create(const char *device_id);

/**
 * @brief Register power state callback
 */
esp_err_t sinricpro_tv_on_power_state(
    sinricpro_device_handle_t device,
    sinricpro_tv_power_state_callback_t callback,
    void *user_data
);

/**
 * @brief Register volume callback
 */
esp_err_t sinricpro_tv_on_volume(
    sinricpro_device_handle_t device,
    sinricpro_tv_volume_callback_t callback,
    void *user_data
);

/**
 * @brief Register adjust volume callback
 */
esp_err_t sinricpro_tv_on_adjust_volume(
    sinricpro_device_handle_t device,
    sinricpro_tv_adjust_volume_callback_t callback,
    void *user_data
);

/**
 * @brief Register mute callback
 */
esp_err_t sinricpro_tv_on_mute(
    sinricpro_device_handle_t device,
    sinricpro_tv_mute_callback_t callback,
    void *user_data
);

/**
 * @brief Register media control callback
 */
esp_err_t sinricpro_tv_on_media_control(
    sinricpro_device_handle_t device,
    sinricpro_tv_media_control_callback_t callback,
    void *user_data
);

/**
 * @brief Register input callback
 */
esp_err_t sinricpro_tv_on_input(
    sinricpro_device_handle_t device,
    sinricpro_tv_input_callback_t callback,
    void *user_data
);

/**
 * @brief Register channel callback
 */
esp_err_t sinricpro_tv_on_channel(
    sinricpro_device_handle_t device,
    sinricpro_tv_channel_callback_t callback,
    void *user_data
);

/**
 * @brief Register skip channels callback
 */
esp_err_t sinricpro_tv_on_skip_channels(
    sinricpro_device_handle_t device,
    sinricpro_tv_skip_channels_callback_t callback,
    void *user_data
);

/**
 * @brief Send power state event
 */
esp_err_t sinricpro_tv_send_power_state_event(
    sinricpro_device_handle_t device,
    bool state,
    const char *cause
);

/**
 * @brief Send volume event
 */
esp_err_t sinricpro_tv_send_volume_event(
    sinricpro_device_handle_t device,
    int volume,
    const char *cause
);

/**
 * @brief Send mute event
 */
esp_err_t sinricpro_tv_send_mute_event(
    sinricpro_device_handle_t device,
    bool mute,
    const char *cause
);

/**
 * @brief Send media control event
 */
esp_err_t sinricpro_tv_send_media_control_event(
    sinricpro_device_handle_t device,
    const char *control,
    const char *cause
);

/**
 * @brief Send input event
 */
esp_err_t sinricpro_tv_send_input_event(
    sinricpro_device_handle_t device,
    const char *input,
    const char *cause
);

/**
 * @brief Send channel event
 */
esp_err_t sinricpro_tv_send_channel_event(
    sinricpro_device_handle_t device,
    const sinricpro_tv_channel_t *channel,
    const char *cause
);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_TV_H */
