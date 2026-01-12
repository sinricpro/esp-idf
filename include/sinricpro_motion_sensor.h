/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_MOTION_SENSOR_H
#define SINRICPRO_MOTION_SENSOR_H

#include "sinricpro_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a MotionSensor device
 *
 * @param[in] device_id Device ID from SinricPro portal (24-character hex string)
 *
 * @return Device handle, or NULL on failure
 */
sinricpro_device_handle_t sinricpro_motion_sensor_create(const char *device_id);

/**
 * @brief Delete a MotionSensor device
 *
 * @param[in] device Device handle
 *
 * @return ESP_OK on success
 */
esp_err_t sinricpro_motion_sensor_delete(sinricpro_device_handle_t device);

/**
 * @brief Send motion detection event
 *
 * @param[in] device   Device handle
 * @param[in] detected true = motion detected, false = no motion
 * @param[in] cause    Cause string (use SINRICPRO_CAUSE_* constants)
 *
 * @return ESP_OK on success
 */
esp_err_t sinricpro_motion_sensor_send_motion_event(
    sinricpro_device_handle_t device,
    bool detected,
    const char *cause);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_MOTION_SENSOR_H */
