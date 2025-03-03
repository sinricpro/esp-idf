#pragma once

/**
 * @file sinricpro_device.h
 * @author SinricPro (ESP-IDF Port)
 * @brief Base class for all SinricPro devices
 */

#include <string>
#include <map>
#include <functional>

#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "sinricpro_config.h"
#include "sinricpro_debug.h"
#include "sinricpro_namespace.h"
#include "sinricpro_device_interface.h"
#include "sinricpro_interface.h"

SINRICPRO_NAMESPACE_BEGIN

// Forward declarations
class sinricpro_t;
class sinricpro_request_t;
class sinricpro_response_t;

/**
 * @typedef request_handler_t
 * @brief Callback function type for handling device requests
 */
using request_handler_t = std::function<bool(sinricpro_request_t&)>;

/**
 * @brief SinricPro request object
 */
class sinricpro_request_t {
public:
  /**
   * @brief Construct a new sinricpro_request_t object
   * @param request_json The JSON object containing the request
   */
  sinricpro_request_t(cJSON* request_json) : json(request_json) {}

  /**
   * @brief Get the device ID from the request
   * @return std::string Device ID
   */
  std::string get_device_id(void) const;

  /**
   * @brief Get the action from the request
   * @return std::string Action name
   */
  std::string get_action(void) const;

  /**
   * @brief Get a value from the request
   * @param key The key to get the value for
   * @return cJSON* The value
   */
  cJSON* get_value(const char* key) const;

  // For internal use
  cJSON* json;
};

/**
 * @brief SinricPro response object
 */
class sinricpro_response_t {
public:
  /**
   * @brief Construct a new sinricpro_response_t object
   * @param request The request to respond to
   */
  sinricpro_response_t(const sinricpro_request_t& request);

  ~sinricpro_response_t();

  /**
   * @brief Set a value in the response
   * @param key The key to set
   * @param value The value to set
   */
  void set_value(const char* key, bool value);
  void set_value(const char* key, int value);
  void set_value(const char* key, float value);
  void set_value(const char* key, const char* value);
  void set_value(const char* key, const std::string& value);

  /**
   * @brief Set the success flag
   * @param success Whether the request was successful
   */
  void set_success(bool success);

  /**
   * @brief Convert the response to a JSON string
   * @return std::string JSON string
   */
  std::string to_string(void) const;

private:
  mutable cJSON* json;
};

/**
 * @brief Base class for all SinricPro devices
 */
class sinricpro_device_t : public sinricpro_device_interface_t {
  friend class sinricpro_t;

public:
  /**
   * @brief Construct a new sinricpro_device_t object
   * @param device_id The device ID
   * @param device_type The device type
   */
  sinricpro_device_t(const std::string& device_id, const std::string& device_type);
  
  /**
   * @brief Destroy the sinricpro_device_t object
   */
  virtual ~sinricpro_device_t(void);

  /**
   * @brief Get the device ID
   * @return std::string The device ID
   */
  std::string get_device_id(void) const override { return device_id; }

  /**
   * @brief Get the device type
   * @return std::string The device type
   */
  std::string get_device_type(void) const { return device_type; }

  /**
   * @brief Initialize the device
   * @param sinric Pointer to the SinricPro instance
   * @return true if initialization was successful
   */
  bool begin(sinricpro_t* sinric) override;

  /**
   * @brief Handle a request for this device
   * @param request_string JSON request as string
   * @return std::string Response as JSON string
   */
  std::string handle_request(const std::string& request_string) override;

  /**
   * @brief Add a request handler
   * @param action The action to handle
   * @param handler The handler function
   */
  void add_request_handler(const std::string& action, request_handler_t handler);

protected:
  /**
   * @brief Prepare an event
   * @param action The action name
   * @param cause The cause of the event
   * @return std::string The event as JSON string
   */
  std::string prepare_event(const std::string& action, const std::string& cause);

  /**
   * @brief Send an event
   * @param event The event as JSON string
   * @return true if successful
   */
  bool send_event(const std::string& event);

  /**
   * @brief Send a push notification
   * @param message The notification message
   * @return true if successful
   */
  bool send_push_notification(const std::string& message);

private:
  std::string device_id;
  std::string device_type;
  std::map<std::string, request_handler_t> request_handlers;
  sinricpro_t* sinric = nullptr;
  SemaphoreHandle_t mutex;

  static constexpr const char* TAG = "SinricProDevice";
};

SINRICPRO_NAMESPACE_END