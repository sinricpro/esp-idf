#pragma once

/**
 * @file sinricpro_switch.h
 * @author SinricPro (ESP-IDF Port)
 * @brief Switch device implementation
 */

#include <string>
#include <functional>

#include "sinricpro_device.h"
#include "capabilities/power_state_controller.h"

SINRICPRO_NAMESPACE_BEGIN

/**
 * @typedef power_state_cb_t
 * @brief Callback function type for power state changes
 * 
 * Parameters:
 * @param device_id Device ID
 * @param state New power state
 * @return true if callback handled correctly
 */
using power_state_cb_t = std::function<bool(const std::string&, bool)>;

/**
 * @brief A switch device that can be turned on and off
 */
class sinricpro_switch_t : 
    public sinricpro_device_t, 
    public power_state_controller_t<sinricpro_switch_t> {
public:
  /**
   * @brief Construct a new sinricpro_switch_t object
   * @param device_id The device ID
   */
  sinricpro_switch_t(const std::string& device_id) 
    : sinricpro_device_t(device_id, "SWITCH") {}

  /**
   * @brief Set the callback for power state changes
   * @param callback The callback function
   */
  void set_power_state_callback(power_state_cb_t callback);

  /**
   * @brief Send a power state event
   * @param state The new power state
   * @param cause The cause of the event (default: "PHYSICAL_INTERACTION")
   * @return true if event was sent successfully
   */
  bool send_power_state_event(bool state, const std::string& cause = "PHYSICAL_INTERACTION");

private:
  // Handler for power state action
  bool handle_power_state(sinricpro_request_t& request);
};

SINRICPRO_NAMESPACE_END