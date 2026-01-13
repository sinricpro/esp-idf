/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef VOLUME_CONTROLLER_H
#define VOLUME_CONTROLLER_H

#include "sinricpro_types.h"
#include "cJSON.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Volume callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] volume Pointer to volume value (0-100)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_volume_callback_t)(
    const char *device_id,
    int *volume,
    void *user_data
);

/**
 * @brief Adjust volume callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] delta Pointer to delta value (-100 to +100)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_adjust_volume_callback_t)(
    const char *device_id,
    int *delta,
    void *user_data
);

typedef struct sinricpro_volume_controller* sinricpro_volume_controller_handle_t;

sinricpro_volume_controller_handle_t sinricpro_volume_controller_create(void);

esp_err_t sinricpro_volume_controller_set_callback(
    sinricpro_volume_controller_handle_t handle,
    sinricpro_volume_callback_t callback,
    void *user_data);

esp_err_t sinricpro_volume_controller_set_adjust_callback(
    sinricpro_volume_controller_handle_t handle,
    sinricpro_adjust_volume_callback_t callback,
    void *user_data);

bool sinricpro_volume_controller_handle_request(
    sinricpro_volume_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value);

esp_err_t sinricpro_volume_controller_send_event(
    sinricpro_volume_controller_handle_t handle,
    const char *device_id,
    int volume,
    const char *cause);

void sinricpro_volume_controller_destroy(sinricpro_volume_controller_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* VOLUME_CONTROLLER_H */
