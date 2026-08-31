/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_MDNS_H
#define SINRICPRO_MDNS_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Announce this device as a local control endpoint
 *
 * Publishes _sinricpro._udp.local. on CONFIG_SINRICPRO_UDP_PORT with the host
 * name sinricpro-<mac> and TXT records deviceIds, sdk and udp=1, so the app can
 * find the device on the LAN without asking the cloud for its address.
 *
 * Compiled out when CONFIG_SINRICPRO_LOCAL_CONTROL_NO_MDNS is set; the UDP
 * listener still works for a client that knows the device address.
 *
 * @param[in] device_ids Comma-separated device ids this board answers for
 *
 * @return ESP_OK if the service was published, an error otherwise
 */
esp_err_t sinricpro_mdns_start(const char *device_ids);

/**
 * @brief Re-announce, but only if the device list actually changed
 *
 * Cheap and idempotent; refreshed on a device list change rather than on a
 * timer or on every reconnect.
 *
 * @param[in] device_ids Comma-separated device ids this board answers for
 */
void sinricpro_mdns_update(const char *device_ids);

/**
 * @brief Withdraw the service record
 *
 * The mDNS responder itself is left running: it is a shared, idempotently
 * initialised service and the application may be advertising on it too.
 */
void sinricpro_mdns_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_MDNS_H */
