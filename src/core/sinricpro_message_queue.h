/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_MESSAGE_QUEUE_H
#define SINRICPRO_MESSAGE_QUEUE_H

#include "esp_err.h"
#include "sinricpro_types.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Transport a message arrived on, and must be answered over
 */
typedef enum {
    SINRICPRO_TRANSPORT_WEBSOCKET = 0,  /**< SinricPro cloud websocket */
    SINRICPRO_TRANSPORT_UDP       = 1,  /**< Local control over UDP */
} sinricpro_transport_t;

/**
 * @brief Where a message came from, and where its response must go
 *
 * Carried per message rather than held in one slot on the listener: a response
 * can leave several queue iterations after the request arrived, by which time
 * another peer may have sent a packet.
 *
 * Held by value in the queue entry, so it adds no allocation to free.
 */
typedef struct {
    sinricpro_transport_t transport;  /**< Transport the message belongs to */
    uint32_t peer_addr;               /**< Peer IPv4, network order (UDP only) */
    uint16_t peer_port;               /**< Peer port, 0 when there is no peer */
} sinricpro_msg_origin_t;

/**
 * @brief Origin of a message that belongs to the cloud websocket
 */
#define SINRICPRO_ORIGIN_WEBSOCKET \
    ((sinricpro_msg_origin_t){ .transport = SINRICPRO_TRANSPORT_WEBSOCKET, \
                               .peer_addr = 0, .peer_port = 0 })

/**
 * @brief Message queue handle (opaque)
 */
typedef struct sinricpro_message_queue* sinricpro_message_queue_handle_t;

/**
 * @brief Create a message queue
 *
 * @param[in] max_size Maximum number of messages in queue
 *
 * @return Queue handle, or NULL on failure
 */
sinricpro_message_queue_handle_t sinricpro_message_queue_create(size_t max_size);

/**
 * @brief Push a message to the queue
 *
 * The message string is copied internally.
 *
 * @param[in] handle  Queue handle
 * @param[in] message Message string to push
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - SINRICPRO_ERR_QUEUE_FULL: Queue is full
 *     - SINRICPRO_ERR_NO_MEMORY: Out of memory
 */
esp_err_t sinricpro_message_queue_push(sinricpro_message_queue_handle_t handle,
                                        const char *message);

/**
 * @brief Push a message together with the origin it must be answered over
 *
 * The message string is copied internally; the origin is stored by value.
 *
 * @param[in] handle  Queue handle
 * @param[in] message Message string to push
 * @param[in] origin  Transport and peer, NULL for the cloud websocket
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - SINRICPRO_ERR_QUEUE_FULL: Queue is full
 *     - SINRICPRO_ERR_NO_MEMORY: Out of memory
 */
esp_err_t sinricpro_message_queue_push_with_origin(sinricpro_message_queue_handle_t handle,
                                                    const char *message,
                                                    const sinricpro_msg_origin_t *origin);

/**
 * @brief Pop a message from the queue
 *
 * The caller is responsible for freeing the returned message using
 * sinricpro_message_queue_free_message().
 *
 * @param[in]  handle   Queue handle
 * @param[out] message  Pointer to receive message string
 * @param[in]  timeout  Timeout in FreeRTOS ticks (portMAX_DELAY = wait forever)
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_TIMEOUT: Timeout waiting for message
 */
esp_err_t sinricpro_message_queue_pop(sinricpro_message_queue_handle_t handle,
                                       char **message,
                                       TickType_t timeout);

/**
 * @brief Pop a message and the origin it must be answered over
 *
 * The caller is responsible for freeing the returned message using
 * sinricpro_message_queue_free_message(), on every path including the ones
 * that discard it.
 *
 * @param[in]  handle   Queue handle
 * @param[out] message  Pointer to receive message string
 * @param[out] origin   Receives the message origin (may be NULL)
 * @param[in]  timeout  Timeout in FreeRTOS ticks (portMAX_DELAY = wait forever)
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_TIMEOUT: Timeout waiting for message
 */
esp_err_t sinricpro_message_queue_pop_with_origin(sinricpro_message_queue_handle_t handle,
                                                   char **message,
                                                   sinricpro_msg_origin_t *origin,
                                                   TickType_t timeout);

/**
 * @brief Free a message returned by sinricpro_message_queue_pop()
 *
 * @param[in] message Message to free
 */
void sinricpro_message_queue_free_message(char *message);

/**
 * @brief Get number of messages in queue
 *
 * @param[in] handle Queue handle
 *
 * @return Number of messages, or 0 if handle is NULL
 */
size_t sinricpro_message_queue_count(sinricpro_message_queue_handle_t handle);

/**
 * @brief Check if queue is empty
 *
 * @param[in] handle Queue handle
 *
 * @return true if empty, false otherwise
 */
bool sinricpro_message_queue_is_empty(sinricpro_message_queue_handle_t handle);

/**
 * @brief Check if queue is full
 *
 * @param[in] handle Queue handle
 *
 * @return true if full, false otherwise
 */
bool sinricpro_message_queue_is_full(sinricpro_message_queue_handle_t handle);

/**
 * @brief Clear all messages from queue
 *
 * @param[in] handle Queue handle
 */
void sinricpro_message_queue_clear(sinricpro_message_queue_handle_t handle);

/**
 * @brief Destroy message queue and free resources
 *
 * @param[in] handle Queue handle
 */
void sinricpro_message_queue_destroy(sinricpro_message_queue_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_MESSAGE_QUEUE_H */
