/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_BLINDS_H
#define SINRICPRO_BLINDS_H

#include "sinricpro_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Power state callback for blinds
 *
 * @param[in] device_id Device ID
 * @param[in,out] state Pointer to power state (true=on, false=off)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_blinds_power_state_callback_t)(
    const char *device_id,
    bool *state,
    void *user_data
);

/**
 * @brief Range value callback for blinds position
 *
 * @param[in] device_id Device ID
 * @param[in,out] range_value Pointer to range value (0-100, 0=closed, 100=open)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_blinds_range_value_callback_t)(
    const char *device_id,
    int *range_value,
    void *user_data
);

/**
 * @brief Adjust range value callback for blinds position
 *
 * @param[in] device_id Device ID
 * @param[in,out] delta Pointer to delta value (-100 to +100)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_blinds_adjust_range_value_callback_t)(
    const char *device_id,
    int *delta,
    void *user_data
);

/**
 * @brief Create a new blinds device
 *
 * @param[in] device_id Device ID from SinricPro portal
 * @return Device handle or NULL on error
 */
sinricpro_device_handle_t sinricpro_blinds_create(const char *device_id);

/**
 * @brief Register power state callback
 *
 * @param[in] device Device handle
 * @param[in] callback Callback function
 * @param[in] user_data User data passed to callback
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_blinds_on_power_state(
    sinricpro_device_handle_t device,
    sinricpro_blinds_power_state_callback_t callback,
    void *user_data
);

/**
 * @brief Register range value callback for position control
 *
 * @param[in] device Device handle
 * @param[in] callback Callback function
 * @param[in] user_data User data passed to callback
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_blinds_on_range_value(
    sinricpro_device_handle_t device,
    sinricpro_blinds_range_value_callback_t callback,
    void *user_data
);

/**
 * @brief Register adjust range value callback
 *
 * @param[in] device Device handle
 * @param[in] callback Callback function
 * @param[in] user_data User data passed to callback
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_blinds_on_adjust_range_value(
    sinricpro_device_handle_t device,
    sinricpro_blinds_adjust_range_value_callback_t callback,
    void *user_data
);

/**
 * @brief Send power state event
 *
 * @param[in] device Device handle
 * @param[in] state Power state (true=on, false=off)
 * @param[in] cause Event cause (PHYSICAL_INTERACTION, etc.)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_blinds_send_power_state_event(
    sinricpro_device_handle_t device,
    bool state,
    const char *cause
);

/**
 * @brief Send range value event for position change
 *
 * @param[in] device Device handle
 * @param[in] range_value Range value (0-100, 0=closed, 100=open)
 * @param[in] cause Event cause (PHYSICAL_INTERACTION, etc.)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_blinds_send_range_value_event(
    sinricpro_device_handle_t device,
    int range_value,
    const char *cause
);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_BLINDS_H */
