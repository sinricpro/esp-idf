/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef THERMOSTAT_CONTROLLER_H
#define THERMOSTAT_CONTROLLER_H

#include "sinricpro_types.h"
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
 * @brief Thermostat mode callback signature
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
 * @brief Target temperature callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] temperature Pointer to target temperature in Celsius
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_target_temperature_callback_t)(
    const char *device_id,
    float *temperature,
    void *user_data
);

/**
 * @brief Adjust target temperature callback signature
 *
 * @param[in] device_id Device ID
 * @param[in,out] delta Pointer to temperature delta in Celsius
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_adjust_target_temperature_callback_t)(
    const char *device_id,
    float *delta,
    void *user_data
);

typedef struct sinricpro_thermostat_controller* sinricpro_thermostat_controller_handle_t;

sinricpro_thermostat_controller_handle_t sinricpro_thermostat_controller_create(void);

esp_err_t sinricpro_thermostat_controller_set_mode_callback(
    sinricpro_thermostat_controller_handle_t handle,
    sinricpro_thermostat_mode_callback_t callback,
    void *user_data);

esp_err_t sinricpro_thermostat_controller_set_target_temperature_callback(
    sinricpro_thermostat_controller_handle_t handle,
    sinricpro_target_temperature_callback_t callback,
    void *user_data);

esp_err_t sinricpro_thermostat_controller_set_adjust_temperature_callback(
    sinricpro_thermostat_controller_handle_t handle,
    sinricpro_adjust_target_temperature_callback_t callback,
    void *user_data);

bool sinricpro_thermostat_controller_handle_request(
    sinricpro_thermostat_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value);

esp_err_t sinricpro_thermostat_controller_send_mode_event(
    sinricpro_thermostat_controller_handle_t handle,
    const char *device_id,
    sinricpro_thermostat_mode_t mode,
    const char *cause);

esp_err_t sinricpro_thermostat_controller_send_target_temperature_event(
    sinricpro_thermostat_controller_handle_t handle,
    const char *device_id,
    float temperature,
    const char *cause);

void sinricpro_thermostat_controller_destroy(sinricpro_thermostat_controller_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* THERMOSTAT_CONTROLLER_H */
