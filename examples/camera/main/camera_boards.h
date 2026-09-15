/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef CAMERA_BOARDS_H
#define CAMERA_BOARDS_H

#include "esp_camera.h"

/** Pin mapping and capture defaults for the board selected in menuconfig. */
camera_config_t camera_board_config(void);

const char *camera_board_name(void);

/** Board-specific GPIO setup that must happen before esp_camera_init(). */
void camera_board_prepare(void);

#endif /* CAMERA_BOARDS_H */
