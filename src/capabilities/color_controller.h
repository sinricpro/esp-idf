/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef COLOR_CONTROLLER_H
#define COLOR_CONTROLLER_H

#include "sinricpro_types.h"
#include "cJSON.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RGB color structure
 */
typedef struct {
    uint8_t r;  /**< Red component (0-255) */
    uint8_t g;  /**< Green component (0-255) */
    uint8_t b;  /**< Blue component (0-255) */
} sinricpro_color_t;

/**
 * @brief Color callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] color Pointer to RGB color structure
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_color_callback_t)(
    const char *device_id,
    sinricpro_color_t *color,
    void *user_data
);

typedef struct sinricpro_color_controller* sinricpro_color_controller_handle_t;

sinricpro_color_controller_handle_t sinricpro_color_controller_create(void);

esp_err_t sinricpro_color_controller_set_callback(
    sinricpro_color_controller_handle_t handle,
    sinricpro_color_callback_t callback,
    void *user_data);

bool sinricpro_color_controller_handle_request(
    sinricpro_color_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value);

esp_err_t sinricpro_color_controller_send_event(
    sinricpro_color_controller_handle_t handle,
    const char *device_id,
    const sinricpro_color_t *color,
    const char *cause);

void sinricpro_color_controller_destroy(sinricpro_color_controller_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* COLOR_CONTROLLER_H */
