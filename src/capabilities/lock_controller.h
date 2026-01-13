/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef LOCK_CONTROLLER_H
#define LOCK_CONTROLLER_H

#include "sinricpro_types.h"
#include "cJSON.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Lock state callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] state true = lock, false = unlock
 * @param[in] user_data User data
 * @return true if successful, false if jammed or error
 */
typedef bool (*sinricpro_lock_state_callback_t)(
    const char *device_id,
    bool *state,
    void *user_data
);

typedef struct sinricpro_lock_controller* sinricpro_lock_controller_handle_t;

sinricpro_lock_controller_handle_t sinricpro_lock_controller_create(void);

esp_err_t sinricpro_lock_controller_set_callback(
    sinricpro_lock_controller_handle_t handle,
    sinricpro_lock_state_callback_t callback,
    void *user_data);

bool sinricpro_lock_controller_handle_request(
    sinricpro_lock_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value);

esp_err_t sinricpro_lock_controller_send_event(
    sinricpro_lock_controller_handle_t handle,
    const char *device_id,
    bool state,
    const char *cause);

void sinricpro_lock_controller_destroy(sinricpro_lock_controller_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* LOCK_CONTROLLER_H */
