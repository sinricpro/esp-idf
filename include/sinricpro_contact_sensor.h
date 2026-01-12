/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_CONTACT_SENSOR_H
#define SINRICPRO_CONTACT_SENSOR_H

#include "sinricpro_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

sinricpro_device_handle_t sinricpro_contact_sensor_create(const char *device_id);

esp_err_t sinricpro_contact_sensor_delete(sinricpro_device_handle_t device);

esp_err_t sinricpro_contact_sensor_send_contact_event(
    sinricpro_device_handle_t device,
    bool detected,
    const char *cause);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_CONTACT_SENSOR_H */
