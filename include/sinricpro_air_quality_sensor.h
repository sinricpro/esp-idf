/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_AIR_QUALITY_SENSOR_H
#define SINRICPRO_AIR_QUALITY_SENSOR_H

#include "sinricpro_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

sinricpro_device_handle_t sinricpro_air_quality_sensor_create(const char *device_id);

esp_err_t sinricpro_air_quality_sensor_delete(sinricpro_device_handle_t device);

esp_err_t sinricpro_air_quality_sensor_send_air_quality_event(
    sinricpro_device_handle_t device,
    int pm1,
    int pm2_5,
    int pm10,
    const char *cause);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_AIR_QUALITY_SENSOR_H */
