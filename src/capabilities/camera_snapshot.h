/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef CAMERA_SNAPSHOT_H
#define CAMERA_SNAPSHOT_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Snapshot upload, kept apart from camera_controller.c so that file stays free of network
 * dependencies and can be compiled by the host tests. */

/**
 * @brief Upload a JPEG snapshot over HTTPS
 *
 * Posts to the SinricPro camera endpoint, signed with the device credentials rather
 * than wrapped in the websocket envelope, matching what the other SinricPro SDKs send.
 *
 * @return ESP_OK when the server accepted the image
 */
esp_err_t sinricpro_camera_snapshot_upload(const char *device_id,
                                           const uint8_t *jpeg,
                                           size_t length);

#ifdef __cplusplus
}
#endif

#endif /* CAMERA_SNAPSHOT_H */
