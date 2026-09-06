/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro_message_queue.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static const char *TAG = "sinricpro_msg_queue";

/**
 * @brief Message queue structure
 */
struct sinricpro_message_queue {
    QueueHandle_t queue;
    size_t max_size;
};

/**
 * @brief Message structure
 */
typedef struct {
    char *message;
    size_t length;
    sinricpro_msg_origin_t origin;
} queue_message_t;

sinricpro_message_queue_handle_t sinricpro_message_queue_create(size_t max_size)
{
    if (max_size == 0) {
        ESP_LOGE(TAG, "Invalid max_size: %zu", max_size);
        return NULL;
    }

    sinricpro_message_queue_handle_t handle = malloc(sizeof(struct sinricpro_message_queue));
    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate message queue handle");
        return NULL;
    }

    handle->max_size = max_size;
    handle->queue = xQueueCreate(max_size, sizeof(queue_message_t));

    if (handle->queue == NULL) {
        ESP_LOGE(TAG, "Failed to create FreeRTOS queue");
        free(handle);
        return NULL;
    }

    ESP_LOGI(TAG, "Message queue created (max_size=%zu)", max_size);

    return handle;
}

esp_err_t sinricpro_message_queue_push(sinricpro_message_queue_handle_t handle,
                                        const char *message)
{
    return sinricpro_message_queue_push_with_origin(handle, message, NULL);
}

esp_err_t sinricpro_message_queue_push_with_origin(sinricpro_message_queue_handle_t handle,
                                                    const char *message,
                                                    const sinricpro_msg_origin_t *origin)
{
    if (handle == NULL || message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Check if queue is full */
    if (uxQueueSpacesAvailable(handle->queue) == 0) {
        ESP_LOGW(TAG, "Message queue is full, dropping message");
        return SINRICPRO_ERR_QUEUE_FULL;
    }

    /* Allocate and copy message */
    size_t msg_len = strlen(message);
    char *msg_copy = malloc(msg_len + 1);
    if (msg_copy == NULL) {
        ESP_LOGE(TAG, "Failed to allocate message buffer");
        return SINRICPRO_ERR_NO_MEMORY;
    }

    strcpy(msg_copy, message);

    queue_message_t queue_msg = {
        .message = msg_copy,
        .length = msg_len,
        .origin = origin ? *origin : SINRICPRO_ORIGIN_WEBSOCKET
    };

    /* Push to queue */
    if (xQueueSend(handle->queue, &queue_msg, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to push message to queue");
        free(msg_copy);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Message pushed to queue (len=%zu, queue_size=%d)",
             msg_len, uxQueueMessagesWaiting(handle->queue));

    return ESP_OK;
}

esp_err_t sinricpro_message_queue_pop(sinricpro_message_queue_handle_t handle,
                                       char **message,
                                       TickType_t timeout)
{
    return sinricpro_message_queue_pop_with_origin(handle, message, NULL, timeout);
}

esp_err_t sinricpro_message_queue_pop_with_origin(sinricpro_message_queue_handle_t handle,
                                                   char **message,
                                                   sinricpro_msg_origin_t *origin,
                                                   TickType_t timeout)
{
    if (handle == NULL || message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    queue_message_t queue_msg;

    if (xQueueReceive(handle->queue, &queue_msg, timeout) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    *message = queue_msg.message;
    if (origin != NULL) {
        *origin = queue_msg.origin;
    }

    ESP_LOGD(TAG, "Message popped from queue (len=%zu, queue_size=%d)",
             queue_msg.length, uxQueueMessagesWaiting(handle->queue));

    return ESP_OK;
}

void sinricpro_message_queue_free_message(char *message)
{
    if (message != NULL) {
        free(message);
    }
}

size_t sinricpro_message_queue_count(sinricpro_message_queue_handle_t handle)
{
    if (handle == NULL) {
        return 0;
    }

    return (size_t)uxQueueMessagesWaiting(handle->queue);
}

bool sinricpro_message_queue_is_empty(sinricpro_message_queue_handle_t handle)
{
    return sinricpro_message_queue_count(handle) == 0;
}

bool sinricpro_message_queue_is_full(sinricpro_message_queue_handle_t handle)
{
    if (handle == NULL) {
        return true;
    }

    return uxQueueSpacesAvailable(handle->queue) == 0;
}

void sinricpro_message_queue_clear(sinricpro_message_queue_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

    queue_message_t queue_msg;

    /* Pop and free all messages */
    while (xQueueReceive(handle->queue, &queue_msg, 0) == pdTRUE) {
        free(queue_msg.message);
    }

    ESP_LOGD(TAG, "Message queue cleared");
}

void sinricpro_message_queue_destroy(sinricpro_message_queue_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

    /* Clear queue first */
    sinricpro_message_queue_clear(handle);

    /* Delete queue */
    vQueueDelete(handle->queue);

    /* Free handle */
    free(handle);

    ESP_LOGI(TAG, "Message queue destroyed");
}
