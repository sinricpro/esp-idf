/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include "sinricpro_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief TemperatureSensor handle (opaque)
 */
typedef struct sinricpro_temperature_sensor* sinricpro_temperature_sensor_handle_t;

/**
 * @brief Create TemperatureSensor
 *
 * @return Sensor handle, or NULL on failure
 */
sinricpro_temperature_sensor_handle_t sinricpro_temperature_sensor_capability_create(void);

/**
 * @brief Send temperature reading event
 *
 * @param[in] handle      Sensor handle
 * @param[in] device_id   Device ID
 * @param[in] temperature Temperature in Celsius
 * @param[in] humidity    Humidity in % (use -1.0f if not available)
 * @param[in] cause       Cause string
 *
 * @return ESP_OK on success
 */
esp_err_t sinricpro_temperature_sensor_send_event(
    sinricpro_temperature_sensor_handle_t handle,
    const char *device_id,
    float temperature,
    float humidity,
    const char *cause);

/**
 * @brief Destroy TemperatureSensor
 *
 * @param[in] handle Sensor handle
 */
void sinricpro_temperature_sensor_destroy(sinricpro_temperature_sensor_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* TEMPERATURE_SENSOR_H */
