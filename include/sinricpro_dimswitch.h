/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_DIMSWITCH_H
#define SINRICPRO_DIMSWITCH_H

#include "sinricpro_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Power state callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] state Pointer to power state (true = on, false = off)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_power_state_callback_t)(
    const char *device_id,
    bool *state,
    void *user_data
);

/**
 * @brief Power level callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] level Pointer to power level (0-100)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_power_level_callback_t)(
    const char *device_id,
    int *level,
    void *user_data
);

/**
 * @brief Adjust power level callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] delta Pointer to delta value (-100 to +100)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_adjust_power_level_callback_t)(
    const char *device_id,
    int *delta,
    void *user_data
);

sinricpro_device_handle_t sinricpro_dimswitch_create(const char *device_id);

esp_err_t sinricpro_dimswitch_delete(sinricpro_device_handle_t device);

esp_err_t sinricpro_dimswitch_on_power_state(
    sinricpro_device_handle_t device,
    sinricpro_power_state_callback_t callback,
    void *user_data);

esp_err_t sinricpro_dimswitch_on_power_level(
    sinricpro_device_handle_t device,
    sinricpro_power_level_callback_t callback,
    void *user_data);

esp_err_t sinricpro_dimswitch_on_adjust_power_level(
    sinricpro_device_handle_t device,
    sinricpro_adjust_power_level_callback_t callback,
    void *user_data);

esp_err_t sinricpro_dimswitch_send_power_state_event(
    sinricpro_device_handle_t device,
    bool state,
    const char *cause);

esp_err_t sinricpro_dimswitch_send_power_level_event(
    sinricpro_device_handle_t device,
    int level,
    const char *cause);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_DIMSWITCH_H */
