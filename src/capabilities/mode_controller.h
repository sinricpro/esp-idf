/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef MODE_CONTROLLER_H
#define MODE_CONTROLLER_H

#include "sinricpro_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mode callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] mode Pointer to mode string (MOVIE, MUSIC, SPORT, etc.)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_mode_callback_t)(
    const char *device_id,
    const char **mode,
    void *user_data
);

typedef struct sinricpro_mode_controller* sinricpro_mode_controller_handle_t;

sinricpro_mode_controller_handle_t sinricpro_mode_controller_create(void);

esp_err_t sinricpro_mode_controller_set_callback(
    sinricpro_mode_controller_handle_t handle,
    sinricpro_mode_callback_t callback,
    void *user_data);

bool sinricpro_mode_controller_handle_request(
    sinricpro_mode_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value);

esp_err_t sinricpro_mode_controller_send_event(
    sinricpro_mode_controller_handle_t handle,
    const char *device_id,
    const char *mode,
    const char *cause);

void sinricpro_mode_controller_destroy(sinricpro_mode_controller_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* MODE_CONTROLLER_H */
