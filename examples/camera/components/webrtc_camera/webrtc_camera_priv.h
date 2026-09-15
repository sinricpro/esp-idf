/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef WEBRTC_CAMERA_PRIV_H
#define WEBRTC_CAMERA_PRIV_H

#include <stdint.h>
#include "esp_timer.h"

/* Wraps after ~49 days; every use is a difference of two readings, which survives the wrap. */
static inline uint32_t webrtc_camera_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

#endif /* WEBRTC_CAMERA_PRIV_H */
