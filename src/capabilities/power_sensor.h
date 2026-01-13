/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef POWER_SENSOR_H
#define POWER_SENSOR_H

#include "sinricpro_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sinricpro_power_sensor* sinricpro_power_sensor_handle_t;

sinricpro_power_sensor_handle_t sinricpro_power_sensor_capability_create(void);

esp_err_t sinricpro_power_sensor_send_event(
    sinricpro_power_sensor_handle_t handle,
    const char *device_id,
    float voltage,
    float current,
    float power,
    float apparent_power,
    float reactive_power,
    float factor,
    const char *cause);

void sinricpro_power_sensor_destroy(sinricpro_power_sensor_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* POWER_SENSOR_H */
