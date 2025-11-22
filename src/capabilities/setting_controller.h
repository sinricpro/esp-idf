/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SETTING_CONTROLLER_H
#define SETTING_CONTROLLER_H

#include "sinricpro_types.h"
#include "sinricpro_switch.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SettingController handle (opaque)
 */
typedef struct sinricpro_setting_controller* sinricpro_setting_controller_handle_t;

/**
 * @brief Create SettingController
 *
 * @return Controller handle, or NULL on failure
 */
sinricpro_setting_controller_handle_t sinricpro_setting_controller_create(void);

/**
 * @brief Set Setting callback
 *
 * @param[in] handle     Controller handle
 * @param[in] callback   Callback function
 * @param[in] user_data  User data
 *
 * @return ESP_OK on success
 */
esp_err_t sinricpro_setting_controller_set_callback(
    sinricpro_setting_controller_handle_t handle,
    sinricpro_setting_callback_t callback,
    void *user_data);

/**
 * @brief Handle Setting request
 *
 * @param[in]     handle         Controller handle
 * @param[in]     device_id      Device ID
 * @param[in]     action         Action name
 * @param[in]     request_value  Request value JSON
 * @param[in,out] response_value Response value JSON
 *
 * @return true if handled, false otherwise
 */
bool sinricpro_setting_controller_handle_request(
    sinricpro_setting_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value);

/**
 * @brief Destroy SettingController
 *
 * @param[in] handle Controller handle
 */
void sinricpro_setting_controller_destroy(sinricpro_setting_controller_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* SETTING_CONTROLLER_H */
