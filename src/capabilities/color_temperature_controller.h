/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef COLOR_TEMPERATURE_CONTROLLER_H
#define COLOR_TEMPERATURE_CONTROLLER_H

#include "sinricpro_types.h"
#include "cJSON.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Color temperature callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] color_temperature Pointer to color temperature in Kelvin (2200-7000)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_color_temperature_callback_t)(
    const char *device_id,
    int *color_temperature,
    void *user_data
);

/**
 * @brief Adjust color temperature callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] delta Pointer to delta value in Kelvin
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_adjust_color_temperature_callback_t)(
    const char *device_id,
    int *delta,
    void *user_data
);

typedef struct sinricpro_color_temperature_controller* sinricpro_color_temperature_controller_handle_t;

sinricpro_color_temperature_controller_handle_t sinricpro_color_temperature_controller_create(void);

esp_err_t sinricpro_color_temperature_controller_set_callback(
    sinricpro_color_temperature_controller_handle_t handle,
    sinricpro_color_temperature_callback_t callback,
    void *user_data);

esp_err_t sinricpro_color_temperature_controller_set_adjust_callback(
    sinricpro_color_temperature_controller_handle_t handle,
    sinricpro_adjust_color_temperature_callback_t callback,
    void *user_data);

bool sinricpro_color_temperature_controller_handle_request(
    sinricpro_color_temperature_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value);

esp_err_t sinricpro_color_temperature_controller_send_event(
    sinricpro_color_temperature_controller_handle_t handle,
    const char *device_id,
    int color_temperature,
    const char *cause);

void sinricpro_color_temperature_controller_destroy(sinricpro_color_temperature_controller_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* COLOR_TEMPERATURE_CONTROLLER_H */
