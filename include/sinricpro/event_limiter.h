#pragma once

/**
 * @file event_limiter.h
 * @author SinricPro (ESP-IDF Port)
 * @brief Utility to limit the frequency of events
 */

#include <stdint.h>
#include "esp_timer.h"

#include "sinricpro_namespace.h"

SINRICPRO_NAMESPACE_BEGIN

/**
 * @brief Limits the frequency of events to prevent flooding the server
 */
class event_limiter_t {
public:
  /**
   * @brief Construct a new event_limiter_t with default rate
   * @param event_milliseconds Minimum milliseconds between events (default: 1000)
   */
  event_limiter_t(unsigned long event_milliseconds = 1000) 
    : min_event_interval(event_milliseconds) {}

  /**
   * @brief Check if an event can be sent
   * @return true if an event can be sent
   */
  bool operator()(void) {
    int64_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    
    if (current_time - last_event_time >= min_event_interval) {
      last_event_time = current_time;
      return true;
    }
    
    return false;
  }

  /**
   * @brief Set the minimum interval between events
   * @param event_milliseconds Minimum milliseconds between events
   */
  void set_limit(unsigned long event_milliseconds) {
    min_event_interval = event_milliseconds;
  }

  /**
   * @brief Reset the event limiter timer
   */
  void reset(void) {
    last_event_time = 0;
  }

private:
  unsigned long min_event_interval;
  int64_t last_event_time = 0;
};

SINRICPRO_NAMESPACE_END