/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef DOOR_CONTROLLER_H
#define DOOR_CONTROLLER_H

#include "sinricpro_types.h"
#include "cJSON.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Door state callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] state true = close, false = open
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_door_state_callback_t)(
    const char *device_id,
    bool *state,
    void *user_data
);

typedef struct sinricpro_door_controller* sinricpro_door_controller_handle_t;

sinricpro_door_controller_handle_t sinricpro_door_controller_create(void);

esp_err_t sinricpro_door_controller_set_callback(
    sinricpro_door_controller_handle_t handle,
    sinricpro_door_state_callback_t callback,
    void *user_data);

bool sinricpro_door_controller_handle_request(
    sinricpro_door_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value);

esp_err_t sinricpro_door_controller_send_event(
    sinricpro_door_controller_handle_t handle,
    const char *device_id,
    bool state,
    const char *cause);

void sinricpro_door_controller_destroy(sinricpro_door_controller_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* DOOR_CONTROLLER_H */
