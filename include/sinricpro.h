/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_H
#define SINRICPRO_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"
#include "sinricpro_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SinricPro SDK version
 */
#define SINRICPRO_VERSION "1.0.1"

/**
 * @brief SinricPro event base
 */
ESP_EVENT_DECLARE_BASE(SINRICPRO_EVENT);

/**
 * @brief SinricPro event IDs
 */
typedef enum {
    SINRICPRO_EVENT_CONNECTED,      /**< Connected to server */
    SINRICPRO_EVENT_DISCONNECTED,   /**< Disconnected from server */
    SINRICPRO_EVENT_ERROR,          /**< Error occurred */
} sinricpro_event_id_t;

/**
 * @brief SinricPro configuration structure
 */
typedef struct {
    const char *app_key;                /**< SinricPro APP_KEY (required) */
    const char *app_secret;             /**< SinricPro APP_SECRET (required) */
    bool auto_reconnect;                /**< Enable auto-reconnection */
    uint32_t reconnect_interval_ms;     /**< Reconnection interval in ms */
    uint32_t heartbeat_interval_ms;     /**< Heartbeat interval in ms (0 = use default) */
} sinricpro_config_t;

/**
 * @brief Initialize SinricPro with configuration
 *
 * This function must be called before any other SinricPro functions.
 *
 * @param[in] config Pointer to configuration structure
 *
 * @return
 *     - ESP_OK: Success
 *     - SINRICPRO_ERR_INVALID_ARG: Invalid argument (NULL config or missing required fields)
 *     - SINRICPRO_ERR_ALREADY_STARTED: Already initialized
 *     - SINRICPRO_ERR_NO_MEMORY: Out of memory
 *
 * @note This function is thread-safe
 */
esp_err_t sinricpro_init(const sinricpro_config_t *config);

/**
 * @brief Start SinricPro (connect to server)
 *
 * Connects to the SinricPro server and starts event processing.
 * The connection happens asynchronously, and connection status
 * can be monitored via SINRICPRO_EVENT_CONNECTED event.
 *
 * @return
 *     - ESP_OK: Success (connection initiated)
 *     - SINRICPRO_ERR_NOT_INITIALIZED: Not initialized
 *     - SINRICPRO_ERR_ALREADY_STARTED: Already started
 *
 * @note This function is thread-safe
 */
esp_err_t sinricpro_start(void);

/**
 * @brief Stop SinricPro (disconnect from server)
 *
 * Disconnects from the server and stops event processing.
 * Any pending messages will be discarded.
 *
 * @return
 *     - ESP_OK: Success
 *     - SINRICPRO_ERR_NOT_INITIALIZED: Not initialized
 *     - SINRICPRO_ERR_NOT_STARTED: Not started
 *
 * @note This function is thread-safe
 */
esp_err_t sinricpro_stop(void);

/**
 * @brief Deinitialize SinricPro and free resources
 *
 * Stops SinricPro (if running) and frees all resources.
 * All device handles become invalid after this call.
 *
 * @return
 *     - ESP_OK: Success
 *     - SINRICPRO_ERR_NOT_INITIALIZED: Not initialized
 *
 * @note This function is thread-safe
 */
esp_err_t sinricpro_deinit(void);

/**
 * @brief Check if connected to server
 *
 * @return true if connected, false otherwise
 *
 * @note This function is thread-safe
 */
bool sinricpro_is_connected(void);

/**
 * @brief Get current timestamp from server
 *
 * Returns the synchronized timestamp from the SinricPro server.
 * Returns 0 if not connected or timestamp not yet received.
 *
 * @return Unix timestamp in seconds, or 0 if not available
 *
 * @note This function is thread-safe
 */
uint32_t sinricpro_get_timestamp(void);

/**
 * @brief Get version string
 *
 * @return Version string in format "X.Y.Z"
 */
const char* sinricpro_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_H */
