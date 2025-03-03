#pragma once

/**
 * @file sinricpro_device_interface.h
 * @author SinricPro (ESP-IDF Port)
 * @brief Interface for SinricPro devices
 */

#include <string>
#include "sinricpro_namespace.h"

SINRICPRO_NAMESPACE_BEGIN

// Forward declaration
class sinricpro_t;

/**
 * @brief Interface that all SinricPro devices must implement
 */
class sinricpro_device_interface_t {
public:
  /**
   * @brief Get the device ID
   * 
   * @return std::string The device ID
   */
  virtual std::string get_device_id(void) const = 0;
  
  /**
   * @brief Initialize the device
   * 
   * @param sinric Pointer to the SinricPro instance
   * @return true if initialization was successful
   */
  virtual bool begin(sinricpro_t* sinric) = 0;
  
  /**
   * @brief Handle a request for this device
   * 
   * @param request_string JSON request as string
   * @return std::string Response as JSON string
   */
  virtual std::string handle_request(const std::string& request_string) = 0;
};

SINRICPRO_NAMESPACE_END