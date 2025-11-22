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

#ifdef __cplusplus
extern "C" {
#endif

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
