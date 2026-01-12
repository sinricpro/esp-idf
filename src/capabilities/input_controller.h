/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef INPUT_CONTROLLER_H
#define INPUT_CONTROLLER_H

#include "sinricpro_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Input callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] input Pointer to input name (HDMI1, HDMI2, AUX, etc.)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_input_callback_t)(
    const char *device_id,
    const char **input,
    void *user_data
);

typedef struct sinricpro_input_controller* sinricpro_input_controller_handle_t;

sinricpro_input_controller_handle_t sinricpro_input_controller_create(void);

esp_err_t sinricpro_input_controller_set_callback(
    sinricpro_input_controller_handle_t handle,
    sinricpro_input_callback_t callback,
    void *user_data);

bool sinricpro_input_controller_handle_request(
    sinricpro_input_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value);

esp_err_t sinricpro_input_controller_send_event(
    sinricpro_input_controller_handle_t handle,
    const char *device_id,
    const char *input,
    const char *cause);

void sinricpro_input_controller_destroy(sinricpro_input_controller_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* INPUT_CONTROLLER_H */
