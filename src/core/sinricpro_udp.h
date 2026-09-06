/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_UDP_H
#define SINRICPRO_UDP_H

#include "esp_err.h"
#include "sinricpro_message_queue.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Called for every datagram received on the local control socket
 *
 * Runs on the listener task, which is also where device callbacks end up, so
 * the task stack must cover signature verification, JSON parsing and the user
 * callback (CONFIG_SINRICPRO_UDP_TASK_STACK_SIZE).
 *
 * @param[in] data    NUL-terminated datagram payload
 * @param[in] length  Payload length in bytes
 * @param[in] origin  Transport and peer the datagram came from
 * @param[in] context Context passed to sinricpro_udp_start()
 */
typedef void (*sinricpro_udp_rx_cb_t)(const char *data,
                                       size_t length,
                                       const sinricpro_msg_origin_t *origin,
                                       void *context);

/**
 * @brief Start the local control listener
 *
 * Binds UDP CONFIG_SINRICPRO_UDP_PORT on INADDR_ANY (so unicast to this host
 * is received too) and joins the SinricPro multicast group. If the group join
 * fails the task keeps retrying, logging each attempt - a silently failed join
 * is indistinguishable from the feature not existing.
 *
 * @param[in] cb      Receive callback
 * @param[in] context Opaque pointer handed back to @p cb
 *
 * @return
 *     - ESP_OK: Listener task started
 *     - ESP_ERR_INVALID_ARG: cb is NULL
 *     - ESP_ERR_INVALID_STATE: Already started
 *     - ESP_FAIL: Task could not be created
 */
esp_err_t sinricpro_udp_start(sinricpro_udp_rx_cb_t cb, void *context);

/**
 * @brief Stop the listener and close the socket
 */
void sinricpro_udp_stop(void);

/**
 * @brief Check whether the socket is bound and the group joined
 *
 * @return true if local control is serving requests
 */
bool sinricpro_udp_is_running(void);

/**
 * @brief Send a reply to the peer that made the request
 *
 * Sent on the listening socket. A separate send-only socket is a known dead
 * end on lwIP - it reports success and puts nothing on the wire.
 *
 * @param[in] message   Serialised, signed response
 * @param[in] peer_addr Peer IPv4 in network byte order
 * @param[in] peer_port Peer port
 *
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE or ESP_FAIL
 */
esp_err_t sinricpro_udp_send(const char *message,
                              uint32_t peer_addr,
                              uint16_t peer_port);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_UDP_H */
