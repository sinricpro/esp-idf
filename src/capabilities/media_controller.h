/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef MEDIA_CONTROLLER_H
#define MEDIA_CONTROLLER_H

#include "sinricpro_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Media control callback signature
 *
 * @param[in] device_id Device ID
 * @param[in] control Media control command (Play, Pause, Stop, Next, Previous, etc.)
 * @param[in] user_data User data
 * @return true if successful, false on error
 */
typedef bool (*sinricpro_media_control_callback_t)(
    const char *device_id,
    const char *control,
    void *user_data
);

typedef struct sinricpro_media_controller* sinricpro_media_controller_handle_t;

sinricpro_media_controller_handle_t sinricpro_media_controller_create(void);

esp_err_t sinricpro_media_controller_set_callback(
    sinricpro_media_controller_handle_t handle,
    sinricpro_media_control_callback_t callback,
    void *user_data);

bool sinricpro_media_controller_handle_request(
    sinricpro_media_controller_handle_t handle,
    const char *device_id,
    const char *action,
    cJSON *request_value,
    cJSON *response_value);

esp_err_t sinricpro_media_controller_send_event(
    sinricpro_media_controller_handle_t handle,
    const char *device_id,
    const char *control,
    const char *cause);

void sinricpro_media_controller_destroy(sinricpro_media_controller_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_CONTROLLER_H */
