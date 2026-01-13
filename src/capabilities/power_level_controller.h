/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef POWER_LEVEL_CONTROLLER_H
#define POWER_LEVEL_CONTROLLER_H

#include "sinricpro_types.h"
#include "cJSON.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

typedef struct sinricpro_power_level_controller* sinricpro_power_level_controller_handle_t;

sinricpro_power_level_controller_handle_t sinricpro_power_level_controller_create(void);

esp_err_t sinricpro_power_level_controller_set_callback(
    sinricpro_power_level_controller_handle_t handle,
    sinricpro_power_level_callback_t callback,
    void *user_data);

esp_err_t sinricpro_power_level_controller_set_adjust_callback(
    sinricpro_power_level_controller_handle_t handle,
    sinricpro_adjust_power_level_callback_t callback,
    void *user_data);

bool sinricpro_power_level_controller_handle_request(
    sinricpro_power_level_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value);

esp_err_t sinricpro_power_level_controller_send_event(
    sinricpro_power_level_controller_handle_t handle,
    const char *device_id,
    int level,
    const char *cause);

void sinricpro_power_level_controller_destroy(sinricpro_power_level_controller_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* POWER_LEVEL_CONTROLLER_H */
