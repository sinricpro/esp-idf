/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_WEBSOCKET_H
#define SINRICPRO_WEBSOCKET_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WebSocket receive callback
 *
 * Called when a message is received from the server.
 *
 * @param[in] data    Message data
 * @param[in] length  Message length
 * @param[in] context User context
 */
typedef void (*sinricpro_ws_receive_callback_t)(const char *data, size_t length, void *context);

/**
 * @brief WebSocket connected callback
 *
 * Called when connected to the server.
 *
 * @param[in] context User context
 */
typedef void (*sinricpro_ws_connected_callback_t)(void *context);

/**
 * @brief WebSocket disconnected callback
 *
 * Called when disconnected from the server.
 *
 * @param[in] context User context
 */
typedef void (*sinricpro_ws_disconnected_callback_t)(void *context);

/**
 * @brief WebSocket callbacks structure
 */
typedef struct {
    sinricpro_ws_receive_callback_t on_receive;
    sinricpro_ws_connected_callback_t on_connected;
    sinricpro_ws_disconnected_callback_t on_disconnected;
    void *context;
} sinricpro_ws_callbacks_t;

/**
 * @brief Initialize WebSocket client
 *
 * @param[in] server_url   Server URL (e.g., "ws.sinric.pro")
 * @param[in] server_port  Server port (e.g., 80)
 * @param[in] app_key      APP_KEY for authentication
 * @param[in] device_ids   Semicolon-separated device IDs
 * @param[in] callbacks    Callback functions
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_NO_MEM: Out of memory
 *     - ESP_FAIL: WebSocket initialization failed
 */
esp_err_t sinricpro_ws_init(const char *server_url,
                             uint16_t server_port,
                             const char *app_key,
                             const char *device_ids,
                             const sinricpro_ws_callbacks_t *callbacks);

/**
 * @brief Start WebSocket connection
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_STATE: Not initialized
 *     - ESP_FAIL: Connection failed
 */
esp_err_t sinricpro_ws_start(void);

/**
 * @brief Stop WebSocket connection
 *
 * @return ESP_OK on success
 */
esp_err_t sinricpro_ws_stop(void);

/**
 * @brief Deinitialize WebSocket client
 *
 * @return ESP_OK on success
 */
esp_err_t sinricpro_ws_deinit(void);

/**
 * @brief Check if WebSocket is connected
 *
 * @return true if connected, false otherwise
 */
bool sinricpro_ws_is_connected(void);

/**
 * @brief Send a message via WebSocket
 *
 * @param[in] message Message string to send
 * @param[in] length  Message length (0 = use strlen)
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_INVALID_STATE: Not connected
 *     - ESP_FAIL: Send failed
 */
esp_err_t sinricpro_ws_send(const char *message, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_WEBSOCKET_H */
