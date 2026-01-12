/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_GARAGE_DOOR_H
#define SINRICPRO_GARAGE_DOOR_H

#include "sinricpro_types.h"
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

sinricpro_device_handle_t sinricpro_garage_door_create(const char *device_id);

esp_err_t sinricpro_garage_door_delete(sinricpro_device_handle_t device);

esp_err_t sinricpro_garage_door_on_door_state(
    sinricpro_device_handle_t device,
    sinricpro_door_state_callback_t callback,
    void *user_data);

esp_err_t sinricpro_garage_door_send_door_state_event(
    sinricpro_device_handle_t device,
    bool state,
    const char *cause);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_GARAGE_DOOR_H */
