/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef BRIGHTNESS_CONTROLLER_H
#define BRIGHTNESS_CONTROLLER_H

#include "sinricpro_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Brightness callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] brightness Pointer to brightness value (0-100)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_brightness_callback_t)(
    const char *device_id,
    int *brightness,
    void *user_data
);

/**
 * @brief Adjust brightness callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] delta Pointer to delta value (-100 to +100)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_adjust_brightness_callback_t)(
    const char *device_id,
    int *delta,
    void *user_data
);

typedef struct sinricpro_brightness_controller* sinricpro_brightness_controller_handle_t;

sinricpro_brightness_controller_handle_t sinricpro_brightness_controller_create(void);

esp_err_t sinricpro_brightness_controller_set_callback(
    sinricpro_brightness_controller_handle_t handle,
    sinricpro_brightness_callback_t callback,
    void *user_data);

esp_err_t sinricpro_brightness_controller_set_adjust_callback(
    sinricpro_brightness_controller_handle_t handle,
    sinricpro_adjust_brightness_callback_t callback,
    void *user_data);

bool sinricpro_brightness_controller_handle_request(
    sinricpro_brightness_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value);

esp_err_t sinricpro_brightness_controller_send_event(
    sinricpro_brightness_controller_handle_t handle,
    const char *device_id,
    int brightness,
    const char *cause);

void sinricpro_brightness_controller_destroy(sinricpro_brightness_controller_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* BRIGHTNESS_CONTROLLER_H */
