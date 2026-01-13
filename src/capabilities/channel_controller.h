/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef CHANNEL_CONTROLLER_H
#define CHANNEL_CONTROLLER_H

#include "sinricpro_types.h"
#include "cJSON.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Channel structure
 */
typedef struct {
    int number;           /**< Channel number */
    const char *name;     /**< Channel name (optional) */
} sinricpro_channel_t;

/**
 * @brief Channel change callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] channel Pointer to channel structure
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_channel_callback_t)(
    const char *device_id,
    sinricpro_channel_t *channel,
    void *user_data
);

/**
 * @brief Skip channels callback signature
 *
 * @param[in] device_id Device ID
 * @param[in] count Number of channels to skip (positive=forward, negative=backward)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_skip_channels_callback_t)(
    const char *device_id,
    int count,
    void *user_data
);

typedef struct sinricpro_channel_controller* sinricpro_channel_controller_handle_t;

sinricpro_channel_controller_handle_t sinricpro_channel_controller_create(void);

esp_err_t sinricpro_channel_controller_set_callback(
    sinricpro_channel_controller_handle_t handle,
    sinricpro_channel_callback_t callback,
    void *user_data);

esp_err_t sinricpro_channel_controller_set_skip_callback(
    sinricpro_channel_controller_handle_t handle,
    sinricpro_skip_channels_callback_t callback,
    void *user_data);

bool sinricpro_channel_controller_handle_request(
    sinricpro_channel_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value);

esp_err_t sinricpro_channel_controller_send_event(
    sinricpro_channel_controller_handle_t handle,
    const char *device_id,
    const sinricpro_channel_t *channel,
    const char *cause);

void sinricpro_channel_controller_destroy(sinricpro_channel_controller_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* CHANNEL_CONTROLLER_H */
