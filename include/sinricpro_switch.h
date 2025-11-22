/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_SWITCH_H
#define SINRICPRO_SWITCH_H

#include "sinricpro.h"
#include "sinricpro_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PowerState callback function signature
 *
 * This callback is invoked when a setPowerState request is received
 * from the server (e.g., Alexa voice command).
 *
 * @param[in]     device_id   Device ID string
 * @param[in,out] state       Pointer to power state (true=ON, false=OFF)
 *                            Input: requested state
 *                            Output: actual state after applying
 * @param[in]     user_data   User data pointer passed during registration
 *
 * @return true if request handled successfully, false otherwise
 *
 * @note This callback is executed in the callback task context
 * @note The callback should be non-blocking and complete quickly
 */
typedef bool (*sinricpro_power_state_callback_t)(
    const char *device_id,
    bool *state,
    void *user_data
);

/**
 * @brief Setting callback function signature
 *
 * This callback is invoked when a setSetting request is received
 * from the server.
 *
 * @param[in] device_id   Device ID string
 * @param[in] setting_id  Setting identifier string
 * @param[in] value       Setting value string
 * @param[in] user_data   User data pointer passed during registration
 *
 * @return true if request handled successfully, false otherwise
 *
 * @note This callback is executed in the callback task context
 * @note The callback should be non-blocking and complete quickly
 */
typedef bool (*sinricpro_setting_callback_t)(
    const char *device_id,
    const char *setting_id,
    const char *value,
    void *user_data
);

/**
 * @brief Create a switch device
 *
 * Creates and registers a new switch device with the specified device ID.
 * The device ID must be obtained from the SinricPro portal.
 *
 * @param[in] device_id Device ID string (exactly 24 hexadecimal characters)
 *
 * @return
 *     - Device handle on success
 *     - NULL on failure (invalid device_id, out of memory, or max devices reached)
 *
 * @note The device_id string is copied internally
 * @note This function must be called after sinricpro_init() but before sinricpro_start()
 * @note This function is thread-safe
 */
sinricpro_device_handle_t sinricpro_switch_create(const char *device_id);

/**
 * @brief Register PowerState callback
 *
 * Registers a callback function to handle setPowerState requests.
 *
 * @param[in] device      Device handle from sinricpro_switch_create()
 * @param[in] callback    Callback function pointer
 * @param[in] user_data   User data pointer (optional, can be NULL)
 *
 * @return
 *     - ESP_OK: Success
 *     - SINRICPRO_ERR_INVALID_DEVICE: Invalid device handle
 *     - SINRICPRO_ERR_INVALID_ARG: NULL callback
 *
 * @note This function is thread-safe
 */
esp_err_t sinricpro_switch_on_power_state(
    sinricpro_device_handle_t device,
    sinricpro_power_state_callback_t callback,
    void *user_data
);

/**
 * @brief Register Setting callback
 *
 * Registers a callback function to handle setSetting requests.
 *
 * @param[in] device      Device handle from sinricpro_switch_create()
 * @param[in] callback    Callback function pointer
 * @param[in] user_data   User data pointer (optional, can be NULL)
 *
 * @return
 *     - ESP_OK: Success
 *     - SINRICPRO_ERR_INVALID_DEVICE: Invalid device handle
 *     - SINRICPRO_ERR_INVALID_ARG: NULL callback
 *
 * @note This function is thread-safe
 */
esp_err_t sinricpro_switch_on_setting(
    sinricpro_device_handle_t device,
    sinricpro_setting_callback_t callback,
    void *user_data
);

/**
 * @brief Send PowerState event to server
 *
 * Sends a setPowerState event to notify the server of a state change
 * (e.g., physical button press).
 *
 * @param[in] device  Device handle from sinricpro_switch_create()
 * @param[in] state   Power state (true=ON, false=OFF)
 * @param[in] cause   Cause string (use SINRICPRO_CAUSE_* constants)
 *
 * @return
 *     - ESP_OK: Success (event queued for sending)
 *     - SINRICPRO_ERR_INVALID_DEVICE: Invalid device handle
 *     - SINRICPRO_ERR_NOT_CONNECTED: Not connected to server
 *     - SINRICPRO_ERR_RATE_LIMITED: Rate limit exceeded (event dropped)
 *     - SINRICPRO_ERR_QUEUE_FULL: Message queue is full
 *
 * @note Events are rate-limited to 1 per second for state changes
 * @note This function is thread-safe
 * @note The event is sent asynchronously
 */
esp_err_t sinricpro_switch_send_power_state_event(
    sinricpro_device_handle_t device,
    bool state,
    const char *cause
);

/**
 * @brief Send push notification to server
 *
 * Sends a push notification message to the user's mobile app.
 *
 * @param[in] device   Device handle from sinricpro_switch_create()
 * @param[in] message  Notification message string
 *
 * @return
 *     - ESP_OK: Success (notification queued for sending)
 *     - SINRICPRO_ERR_INVALID_DEVICE: Invalid device handle
 *     - SINRICPRO_ERR_INVALID_ARG: NULL message
 *     - SINRICPRO_ERR_NOT_CONNECTED: Not connected to server
 *     - SINRICPRO_ERR_QUEUE_FULL: Message queue is full
 *
 * @note This function is thread-safe
 * @note The notification is sent asynchronously
 */
esp_err_t sinricpro_switch_send_notification(
    sinricpro_device_handle_t device,
    const char *message
);

/**
 * @brief Delete switch device
 *
 * Deletes a switch device and frees associated resources.
 * The device handle becomes invalid after this call.
 *
 * @param[in] device Device handle from sinricpro_switch_create()
 *
 * @return
 *     - ESP_OK: Success
 *     - SINRICPRO_ERR_INVALID_DEVICE: Invalid device handle
 *
 * @note This function should be called before sinricpro_deinit()
 * @note This function is thread-safe
 */
esp_err_t sinricpro_switch_delete(sinricpro_device_handle_t device);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_SWITCH_H */
