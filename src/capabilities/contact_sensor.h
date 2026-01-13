/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef CONTACT_SENSOR_H
#define CONTACT_SENSOR_H

#include "sinricpro_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ContactSensor handle (opaque)
 */
typedef struct sinricpro_contact_sensor* sinricpro_contact_sensor_handle_t;

/**
 * @brief Create ContactSensor
 *
 * @return Sensor handle, or NULL on failure
 */
sinricpro_contact_sensor_handle_t sinricpro_contact_sensor_capability_create(void);

/**
 * @brief Send contact detection event
 *
 * @param[in] handle    Sensor handle
 * @param[in] device_id Device ID
 * @param[in] detected  true = contact closed, false = contact open
 * @param[in] cause     Cause string
 *
 * @return ESP_OK on success
 */
esp_err_t sinricpro_contact_sensor_send_event(
    sinricpro_contact_sensor_handle_t handle,
    const char *device_id,
    bool detected,
    const char *cause);

/**
 * @brief Destroy ContactSensor
 *
 * @param[in] handle Sensor handle
 */
void sinricpro_contact_sensor_destroy(sinricpro_contact_sensor_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* CONTACT_SENSOR_H */
