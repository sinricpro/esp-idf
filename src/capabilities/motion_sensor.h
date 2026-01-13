/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef MOTION_SENSOR_H
#define MOTION_SENSOR_H

#include "sinricpro_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MotionSensor handle (opaque)
 */
typedef struct sinricpro_motion_sensor* sinricpro_motion_sensor_handle_t;

/**
 * @brief Create MotionSensor
 *
 * @return Controller handle, or NULL on failure
 */
sinricpro_motion_sensor_handle_t sinricpro_motion_sensor_capability_create(void);

/**
 * @brief Send motion detection event
 *
 * @param[in] handle    Sensor handle
 * @param[in] device_id Device ID
 * @param[in] detected  true = motion detected, false = no motion
 * @param[in] cause     Cause string
 *
 * @return ESP_OK on success
 */
esp_err_t sinricpro_motion_sensor_send_event(
    sinricpro_motion_sensor_handle_t handle,
    const char *device_id,
    bool detected,
    const char *cause);

/**
 * @brief Destroy MotionSensor
 *
 * @param[in] handle Sensor handle
 */
void sinricpro_motion_sensor_destroy(sinricpro_motion_sensor_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_SENSOR_H */
