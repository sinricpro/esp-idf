/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef AIR_QUALITY_SENSOR_H
#define AIR_QUALITY_SENSOR_H

#include "sinricpro_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sinricpro_air_quality_sensor* sinricpro_air_quality_sensor_handle_t;

sinricpro_air_quality_sensor_handle_t sinricpro_air_quality_sensor_create(void);

esp_err_t sinricpro_air_quality_sensor_send_event(
    sinricpro_air_quality_sensor_handle_t handle,
    const char *device_id,
    int pm1,
    int pm2_5,
    int pm10,
    const char *cause);

void sinricpro_air_quality_sensor_destroy(sinricpro_air_quality_sensor_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* AIR_QUALITY_SENSOR_H */
