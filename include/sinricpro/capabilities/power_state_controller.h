#pragma once

/**
 * @file power_state_controller.h
 * @author SinricPro (ESP-IDF Port)
 * @brief Capability to control power state (on/off)
 */

#include <string>
#include <functional>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "../sinricpro_namespace.h"
#include "../sinricpro_device.h"
#include "../event_limiter.h"

SINRICPRO_NAMESPACE_BEGIN

// Action strings
#define SINRICPRO_POWERSTATE_ACTION "setPowerState"
#define SINRICPRO_POWERSTATE_STATE  "state"
#define SINRICPRO_POWERSTATE_ON     "On"
#define SINRICPRO_POWERSTATE_OFF    "Off"

/**
 * @brief PowerStateController capability
 * 
 * Adds power state control functionality to a device
 */
template <typename device_type_t>
class power_state_controller_t {
public:
  using power_state_cb_t = std::function<bool(const std::string&, bool)>;

  /**
   * @brief Register callback for power state events
   * @param callback The callback function
   */
  void set_power_state_callback(power_state_cb_t callback) {
    power_state_callback = callback;
  }

  /**
   * @brief Send a power state event
   * @param state The new power state
   * @param cause The cause of the event
   * @return true if event was sent successfully
   */
  bool send_power_state_event(bool state, const std::string& cause) {
    auto* device = static_cast<device_type_t*>(this);
    
    // Create JSON for event
    std::string event = device->prepare_event(SINRICPRO_POWERSTATE_ACTION, cause);
    
    // Add power state value to the event
    cJSON* json = cJSON_Parse(event.c_str());
    if (!json) {
      ESP_LOGE("PowerState", "Failed to parse JSON");
      return false;
    }
    
    cJSON* payload = cJSON_GetObjectItem(json, "payload");
    cJSON* value = cJSON_GetObjectItem(payload, "value");
    cJSON_AddStringToObject(value, SINRICPRO_POWERSTATE_STATE, state ? SINRICPRO_POWERSTATE_ON : SINRICPRO_POWERSTATE_OFF);
    
    char* event_string = cJSON_PrintUnformatted(json);
    std::string result = event_string;
    
    cJSON_free(event_string);
    cJSON_Delete(json);
    
    // Send the event
    return device->send_event(result);
  }

  /**
   * @brief Handle a power state request
   * @param request The request to handle
   * @return true if the request was handled successfully
   */
  bool handle_power_state(sinricpro_request_t& request) {
    auto* device = static_cast<device_type_t*>(this);
    
    // Get the state value from the request
    cJSON* state_value = request.get_value(SINRICPRO_POWERSTATE_STATE);
    if (!state_value || !cJSON_IsString(state_value)) {
      return false;
    }
    
    // Convert the state string to a boolean
    bool state = (strcmp(cJSON_GetStringValue(state_value), SINRICPRO_POWERSTATE_ON) == 0);
    
    // Call the callback if registered
    if (power_state_callback) {
      return power_state_callback(device->get_device_id(), state);
    }
    
    return false;
  }

protected:
  power_state_cb_t power_state_callback;
};

SINRICPRO_NAMESPACE_END