/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef PUSH_NOTIFICATION_H
#define PUSH_NOTIFICATION_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Send push notification
 *
 * @param[in] device_id Device ID
 * @param[in] message   Notification message
 *
 * @return ESP_OK on success
 */
esp_err_t sinricpro_send_push_notification(const char *device_id, const char *message);

#ifdef __cplusplus
}
#endif

#endif /* PUSH_NOTIFICATION_H */
