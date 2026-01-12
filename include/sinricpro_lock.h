/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_LOCK_H
#define SINRICPRO_LOCK_H

#include "sinricpro_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Lock state callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] state Pointer to lock state (true = lock, false = unlock)
 * @param[in] user_data User data
 * @return true if successful, false if jammed or error
 */
typedef bool (*sinricpro_lock_state_callback_t)(
    const char *device_id,
    bool *state,
    void *user_data
);

sinricpro_device_handle_t sinricpro_lock_create(const char *device_id);

esp_err_t sinricpro_lock_delete(sinricpro_device_handle_t device);

esp_err_t sinricpro_lock_on_lock_state(
    sinricpro_device_handle_t device,
    sinricpro_lock_state_callback_t callback,
    void *user_data);

esp_err_t sinricpro_lock_send_lock_state_event(
    sinricpro_device_handle_t device,
    bool state,
    const char *cause);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_LOCK_H */
