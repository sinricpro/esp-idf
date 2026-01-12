/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef POWER_STATE_CONTROLLER_H
#define POWER_STATE_CONTROLLER_H

#include "sinricpro_types.h"
#include "sinricpro_switch.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PowerStateController handle (opaque)
 */
typedef struct sinricpro_power_state_controller* sinricpro_power_state_controller_handle_t;

/**
 * @brief Create PowerStateController
 *
 * @return Controller handle, or NULL on failure
 */
sinricpro_power_state_controller_handle_t sinricpro_power_state_controller_create(void);

/**
 * @brief Set PowerState callback
 *
 * @param[in] handle     Controller handle
 * @param[in] callback   Callback function
 * @param[in] user_data  User data
 *
 * @return ESP_OK on success
 */
esp_err_t sinricpro_power_state_controller_set_callback(
    sinricpro_power_state_controller_handle_t handle,
    sinricpro_power_state_callback_t callback,
    void *user_data);

/**
 * @brief Handle PowerState request
 *
 * @param[in]     handle         Controller handle
 * @param[in]     device_id      Device ID
 * @param[in]     action         Action name
 * @param[in]     request_value  Request value JSON
 * @param[in,out] response_value Response value JSON
 *
 * @return true if handled, false otherwise
 */
bool sinricpro_power_state_controller_handle_request(
    sinricpro_power_state_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value);

/**
 * @brief Send PowerState event
 *
 * @param[in] handle    Controller handle
 * @param[in] device_id Device ID
 * @param[in] state     Power state
 * @param[in] cause     Cause string
 *
 * @return ESP_OK on success
 */
esp_err_t sinricpro_power_state_controller_send_event(
    sinricpro_power_state_controller_handle_t handle,
    const char *device_id,
    bool state,
    const char *cause);

/**
 * @brief Destroy PowerStateController
 *
 * @param[in] handle Controller handle
 */
void sinricpro_power_state_controller_destroy(sinricpro_power_state_controller_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* POWER_STATE_CONTROLLER_H */
