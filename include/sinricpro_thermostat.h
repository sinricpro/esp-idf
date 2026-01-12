/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_THERMOSTAT_H
#define SINRICPRO_THERMOSTAT_H

#include "sinricpro_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Thermostat mode enum
 */
typedef enum {
    SINRICPRO_THERMOSTAT_MODE_AUTO,
    SINRICPRO_THERMOSTAT_MODE_COOL,
    SINRICPRO_THERMOSTAT_MODE_HEAT,
    SINRICPRO_THERMOSTAT_MODE_ECO,
    SINRICPRO_THERMOSTAT_MODE_OFF
} sinricpro_thermostat_mode_t;

/**
 * @brief Power state callback for thermostat
 *
 * @param[in] device_id Device ID
 * @param[in,out] state Pointer to power state (true=on, false=off)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_thermostat_power_state_callback_t)(
    const char *device_id,
    bool *state,
    void *user_data
);

/**
 * @brief Thermostat mode callback
 *
 * @param[in] device_id Device ID
 * @param[in,out] mode Pointer to thermostat mode
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_thermostat_mode_callback_t)(
    const char *device_id,
    sinricpro_thermostat_mode_t *mode,
    void *user_data
);

/**
 * @brief Target temperature callback
 *
 * @param[in] device_id Device ID
 * @param[in,out] temperature Pointer to target temperature in Celsius
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_thermostat_target_temperature_callback_t)(
    const char *device_id,
    float *temperature,
    void *user_data
);

/**
 * @brief Adjust target temperature callback
 *
 * @param[in] device_id Device ID
 * @param[in,out] delta Pointer to temperature delta in Celsius
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_thermostat_adjust_temperature_callback_t)(
    const char *device_id,
    float *delta,
    void *user_data
);

/**
 * @brief Create a new thermostat device
 *
 * @param[in] device_id Device ID from SinricPro portal
 * @return Device handle or NULL on error
 */
sinricpro_device_handle_t sinricpro_thermostat_create(const char *device_id);

/**
 * @brief Register power state callback
 *
 * @param[in] device Device handle
 * @param[in] callback Callback function
 * @param[in] user_data User data passed to callback
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_thermostat_on_power_state(
    sinricpro_device_handle_t device,
    sinricpro_thermostat_power_state_callback_t callback,
    void *user_data
);

/**
 * @brief Register thermostat mode callback
 *
 * @param[in] device Device handle
 * @param[in] callback Callback function
 * @param[in] user_data User data passed to callback
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_thermostat_on_thermostat_mode(
    sinricpro_device_handle_t device,
    sinricpro_thermostat_mode_callback_t callback,
    void *user_data
);

/**
 * @brief Register target temperature callback
 *
 * @param[in] device Device handle
 * @param[in] callback Callback function
 * @param[in] user_data User data passed to callback
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_thermostat_on_target_temperature(
    sinricpro_device_handle_t device,
    sinricpro_thermostat_target_temperature_callback_t callback,
    void *user_data
);

/**
 * @brief Register adjust target temperature callback
 *
 * @param[in] device Device handle
 * @param[in] callback Callback function
 * @param[in] user_data User data passed to callback
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_thermostat_on_adjust_target_temperature(
    sinricpro_device_handle_t device,
    sinricpro_thermostat_adjust_temperature_callback_t callback,
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
esp_err_t sinricpro_thermostat_send_power_state_event(
    sinricpro_device_handle_t device,
    bool state,
    const char *cause
);

/**
 * @brief Send thermostat mode event
 *
 * @param[in] device Device handle
 * @param[in] mode Thermostat mode
 * @param[in] cause Event cause (PHYSICAL_INTERACTION, etc.)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_thermostat_send_mode_event(
    sinricpro_device_handle_t device,
    sinricpro_thermostat_mode_t mode,
    const char *cause
);

/**
 * @brief Send target temperature event
 *
 * @param[in] device Device handle
 * @param[in] temperature Target temperature in Celsius
 * @param[in] cause Event cause (PHYSICAL_INTERACTION, etc.)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_thermostat_send_target_temperature_event(
    sinricpro_device_handle_t device,
    float temperature,
    const char *cause
);

/**
 * @brief Send current temperature and humidity event
 *
 * @param[in] device Device handle
 * @param[in] temperature Current temperature in Celsius
 * @param[in] humidity Current humidity in percentage
 * @param[in] cause Event cause (PHYSICAL_INTERACTION, etc.)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sinricpro_thermostat_send_temperature_event(
    sinricpro_device_handle_t device,
    float temperature,
    float humidity,
    const char *cause
);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_THERMOSTAT_H */
