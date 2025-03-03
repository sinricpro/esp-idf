#pragma once

/**
 * @file sinricpro_websocket.h
 * @author SinricPro (ESP-IDF Port)
 * @brief WebSocket client implementation for SinricPro using ESP-IDF's WebSocket client
 */

#include <string>
#include <functional>
#include <memory>

#include "esp_websocket_client.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "sinricpro_config.h"
#include "sinricpro_debug.h"
#include "sinricpro_namespace.h"
#include "sinricpro_queue.h"

SINRICPRO_NAMESPACE_BEGIN

// WebSocket events
typedef enum {
  SINRICPRO_WS_CONNECTED,
  SINRICPRO_WS_DISCONNECTED,
  SINRICPRO_WS_DATA,
  SINRICPRO_WS_ERROR,
} sinricpro_ws_event_t;

// WebSocket event data
typedef struct {
  sinricpro_ws_event_t event;
  void* data;
  size_t data_len;
} sinricpro_ws_event_data_t;

// Callback types
using websocket_message_cb_t = std::function<void(const std::string& message)>;
using websocket_connected_cb_t = std::function<void(void)>;
using websocket_disconnected_cb_t = std::function<void(void)>;

/**
 * @class sinricpro_websocket_t
 * @brief WebSocket client implementation for SinricPro
 */
class sinricpro_websocket_t {
public:
  /**
   * @brief Construct a new sinricpro_websocket_t object
   */
  sinricpro_websocket_t(void);

  /**
   * @brief Destroy the sinricpro_websocket_t object
   */
  ~sinricpro_websocket_t(void);

  /**
   * @brief Begin WebSocket connection
   * @param app_key SinricPro application key
   * @param app_secret SinricPro application secret
   * @param server_url SinricPro server URL
   * @param device_ids Comma-separated list of device IDs
   * @param restore_device_states Whether to restore device states
   * @return true if connection setup is successful
   */
  bool begin(const std::string& app_key, 
             const std::string& app_secret,
             const std::string& server_url, 
             const std::string& device_ids,
             bool restore_device_states = false);

  /**
   * @brief Stop the WebSocket connection
   */
  void stop(void);

  /**
   * @brief Send a message through the WebSocket connection
   * @param message Message to send
   * @return true if message was queued successfully
   */
  bool send_message(const std::string& message);

  /**
   * @brief Check if the WebSocket connection is connected
   * @return true if connected
   */
  bool is_connected(void) const;

  /**
   * @brief Set callback for WebSocket message event
   * @param callback Callback function
   */
  void set_message_callback(websocket_message_cb_t callback);

  /**
   * @brief Set callback for WebSocket connected event
   * @param callback Callback function
   */
  void set_connected_callback(websocket_connected_cb_t callback);

  /**
   * @brief Set callback for WebSocket disconnected event
   * @param callback Callback function
   */
  void set_disconnected_callback(websocket_disconnected_cb_t callback);

  /**
   * @brief Handle WebSocket events (called from task)
   */
  void loop(void);

private:
  // ESP-IDF WebSocket client handle
  esp_websocket_client_handle_t client;
  
  // Connection state
  bool connected;
  EventGroupHandle_t event_group;
  SemaphoreHandle_t mutex;
  
  // Message queue
  sinricpro_queue_t<std::string> send_queue;
  
  // Credentials
  std::string app_key;
  std::string app_secret;
  std::string server_url;
  std::string device_ids;
  bool restore_device_states;
  
  // Callbacks
  websocket_message_cb_t message_callback;
  websocket_connected_cb_t connected_callback;
  websocket_disconnected_cb_t disconnected_callback;
  
  // ESP-IDF event handler
  static void websocket_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
  
  // Build the WebSocket URL with appropriate parameters
  std::string build_websocket_url(void) const;
  
  // Build HTTP headers for WebSocket connection
  esp_websocket_client_config_t build_websocket_config(void) const;
  
  // Process messages in the send queue
  void process_queue(void);
  
  // Logging
  static constexpr const char* TAG = "SinricProWS";
};

SINRICPRO_NAMESPACE_END