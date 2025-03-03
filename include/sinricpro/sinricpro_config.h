#pragma once

/**
 * @file sinricpro_config.h
 * @author SinricPro (ESP-IDF Port)
 * @brief Configuration definitions for SinricPro ESP-IDF component
 */

#include "sinricpro_version.h"

// Server configuration
#define SINRICPRO_SERVER_URL           "ws.sinric.pro"
#define SINRICPRO_SERVER_PORT          80
#define SINRICPRO_SERVER_SSL_PORT      443

// Use secure connections by default
#define SINRICPRO_SECURE_CONNECTION    true

// WebSocket configuration
#define SINRICPRO_WEBSOCKET_RETRY_INTERVAL   5000  // Retry interval in ms
#define SINRICPRO_PING_INTERVAL             10000  // Send ping every 10 seconds
#define SINRICPRO_KEEPALIVE_INTERVAL        15000  // Keep-alive interval
#define SINRICPRO_RESPONSE_TIMEOUT          10000  // Response timeout in ms

// Task configuration
#define SINRICPRO_TASK_STACK_SIZE           4096   // Stack size for the WebSocket task
#define SINRICPRO_TASK_PRIORITY               10   // Priority for the WebSocket task (higher than idle but lower than app)
#define SINRICPRO_TASK_CORE                    1   // Core to run the WebSocket task on (ESP32 only)

// Event group bits
#define SINRICPRO_EVENT_CONNECTED           (1 << 0)
#define SINRICPRO_EVENT_DISCONNECTED        (1 << 1)
#define SINRICPRO_EVENT_MESSAGE_RECEIVED    (1 << 2)
#define SINRICPRO_EVENT_MESSAGE_SENT        (1 << 3)
#define SINRICPRO_EVENT_SHUTDOWN            (1 << 4)

// Debug configuration
#ifdef CONFIG_SINRICPRO_DEBUG
    #define SINRICPRO_DEBUG 1
#else
    #define SINRICPRO_DEBUG 0
#endif

// Maximum limits
#define SINRICPRO_MAX_DEVICES                 10   // Maximum number of devices
#define SINRICPRO_MAX_QUEUE_SIZE              25   // Maximum queue size for messages
#define SINRICPRO_MAX_MESSAGE_SIZE          8192   // Maximum size of a message