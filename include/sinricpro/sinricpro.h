#pragma once

/**
 * @file sinricpro.h
 * @author SinricPro (ESP-IDF Port)
 * @brief SinricPro main interface 
 */

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <map>

#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "cJSON.h"
#include "nvs_flash.h"

#include "sinricpro_config.h"
#include "sinricpro_debug.h"
#include "sinricpro_namespace.h"
#include "sinricpro_queue.h"
#include "sinricpro_interface.h"
#include "sinricpro_device_interface.h"
#include "sinricpro_websocket.h"
#include "sinricpro_signature.h"
#include "sinricpro_device.h"

SINRICPRO_NAMESPACE_BEGIN

using connected_callback_t = std::function<void(void)>;
using disconnected_callback_t = std::function<void(void)>;

/**
 * @class sinricpro_t
 * @brief The main SinricPro class for ESP-IDF
 * 
 * Provides the main interface for connecting to SinricPro services,
 * managing devices, and handling events
 */
class sinricpro_t {
  friend class sinricpro_device_t;

public:
  /**
   * @brief Get the singleton instance of sinricpro_t
   * @return sinricpro_t& Reference to the singleton instance
   */
  static sinricpro_t& get_instance(void);

  /**
   * @brief Initialize SinricPro with app credentials
   * @param app_key Application key from SinricPro portal
   * @param app_secret Application secret from SinricPro portal
   * @param server_url Optional server URL (defaults to SinricPro production server)
   * @return true if initialization was successful
   */
  bool begin(const std::string& app_key, const std::string& app_secret, const std::string& server_url = SINRICPRO_SERVER_URL);

  /**
   * @brief Stop SinricPro service
   */
  void stop(void);

  /**
   * @brief Check if SinricPro is connected to the server
   * @return true if connected, false otherwise
   */
  bool is_connected(void) const;

  /**
   * @brief Set callback for connected event
   * @param cb Callback function to be called when connected
   */
  void set_connected_callback(connected_callback_t cb);

  /**
   * @brief Set callback for disconnected event
   * @param cb Callback function to be called when disconnected
   */
  void set_disconnected_callback(disconnected_callback_t cb);

  /**
   * @brief Add a device to SinricPro
   * @tparam device_type_t Type of device to add
   * @param device_id Device ID from SinricPro portal
   * @return Reference to the added device
   */
  template <typename device_type_t>
  device_type_t& add(const std::string& device_id);

  /**
   * @brief Get a device by its ID
   * @tparam device_type_t Type of device to get
   * @param device_id Device ID from SinricPro portal
   * @return Reference to the device
   */
  template <typename device_type_t>
  device_type_t& operator[](const std::string& device_id);

  /**
   * @brief Process incoming messages (manually)
   * 
   * Note: This is typically not needed as the internal task handles messages
   */
  void handle(void);
  
  /**
   * @brief Send a message to the SinricPro server
   * @param message The message to send
   * @return true if the message was sent successfully
   */
  bool send_message(const std::string& message);

private:
  sinricpro_t(void); // Private constructor (singleton)
  ~sinricpro_t(void);

  // Prevent copying
  sinricpro_t(const sinricpro_t&) = delete;
  sinricpro_t& operator=(const sinricpro_t&) = delete;

  bool handle_receive_queue(void);
  bool handle_send_queue(void);
  void handle_websocket_events(void);
  void start_websocket_task(void);
  
  // Task handlers
  static void websocket_task_handler(void* pvParameters);
  
  // Device management
  std::vector<sinricpro_device_t*> devices;
  sinricpro_device_t* get_device(const std::string& device_id);

  // Websocket connection
  sinricpro_websocket_t* websocket;

  // Internal state
  bool begin_called;
  std::string app_key;
  std::string app_secret;
  std::string server_url;

  // Event handling
  EventGroupHandle_t event_group;
  sinricpro_queue_t<std::string> receive_queue;
  sinricpro_queue_t<std::string> send_queue;

  // Tasks
  TaskHandle_t websocket_task; 

  // Callbacks
  connected_callback_t on_connected_cb;
  disconnected_callback_t on_disconnected_cb;

  // Logging
  static constexpr const char* TAG = "SinricPro";
};

// Template implementation
template <typename device_type_t>
device_type_t& sinricpro_t::add(const std::string& device_id) {
  device_type_t* new_device = new device_type_t(device_id);
  ESP_LOGI(TAG, "Adding device with id \"%s\"", device_id.c_str());
  new_device->begin(this);
  devices.push_back(new_device);
  return *new_device;
}

template <typename device_type_t>
device_type_t& sinricpro_t::operator[](const std::string& device_id) {
  for (auto& device : devices) {
    if (device->get_device_id() == device_id) {
      return *static_cast<device_type_t*>(device);
    }
  }
  return add<device_type_t>(device_id);
}

// Singleton instance
extern sinricpro_t& SinricPro;

SINRICPRO_NAMESPACE_END