/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef MUTE_CONTROLLER_H
#define MUTE_CONTROLLER_H

#include "sinricpro_types.h"
#include "cJSON.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mute callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] mute Pointer to mute state (true=muted, false=unmuted)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_mute_callback_t)(
    const char *device_id,
    bool *mute,
    void *user_data
);

typedef struct sinricpro_mute_controller* sinricpro_mute_controller_handle_t;

sinricpro_mute_controller_handle_t sinricpro_mute_controller_create(void);

esp_err_t sinricpro_mute_controller_set_callback(
    sinricpro_mute_controller_handle_t handle,
    sinricpro_mute_callback_t callback,
    void *user_data);

bool sinricpro_mute_controller_handle_request(
    sinricpro_mute_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value);

esp_err_t sinricpro_mute_controller_send_event(
    sinricpro_mute_controller_handle_t handle,
    const char *device_id,
    bool mute,
    const char *cause);

void sinricpro_mute_controller_destroy(sinricpro_mute_controller_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* MUTE_CONTROLLER_H */
