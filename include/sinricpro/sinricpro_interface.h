#pragma once

/**
 * @file sinricpro_interface.h
 * @author SinricPro (ESP-IDF Port)
 * @brief Interface definition for SinricPro
 */

#include <string>
#include "sinricpro_namespace.h"

SINRICPRO_NAMESPACE_BEGIN

/**
 * @brief Interface for SinricPro event sender
 */
class sinricpro_interface_t {
public:
  /**
   * @brief Prepare an event message
   * 
   * @param device_id The device ID
   * @param action The action name
   * @param cause The cause of the event
   * @return std::string JSON string containing the event
   */
  virtual std::string prepare_event(const std::string& device_id, const std::string& action, const std::string& cause) = 0;
  
  /**
   * @brief Send an event
   * 
   * @param event The event to send as JSON string
   * @return true if successful
   */
  virtual bool send_event(const std::string& event) = 0;
  
  /**
   * @brief Send a message
   * 
   * @param message The message to send
   * @return true if successful
   */
  virtual bool send_message(const std::string& message) = 0;
  
  /**
   * @brief Check if connected to SinricPro server
   * 
   * @return true if connected
   */
  virtual bool is_connected(void) const = 0;
};

SINRICPRO_NAMESPACE_END