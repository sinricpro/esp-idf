#pragma once

/**
 * @file sinricpro_queue.h
 * @author SinricPro (ESP-IDF Port)
 * @brief Thread-safe queue implementation for SinricPro
 */

#include <deque>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sinricpro_config.h"
#include "sinricpro_debug.h"
#include "sinricpro_namespace.h"

SINRICPRO_NAMESPACE_BEGIN

/**
 * @brief Thread-safe queue implementation using FreeRTOS primitives
 * 
 * @tparam T The type of elements in the queue
 */
template <typename T>
class sinricpro_queue_t {
public:
  /**
   * @brief Construct a new sinricpro_queue_t object
   * 
   * @param max_size Maximum size of the queue
   */
  sinricpro_queue_t(size_t max_size = SINRICPRO_MAX_QUEUE_SIZE) 
    : max_size(max_size) {
    mutex = xSemaphoreCreateMutex();
  }

  /**
   * @brief Destroy the sinricpro_queue_t object
   */
  ~sinricpro_queue_t(void) {
    vSemaphoreDelete(mutex);
  }

  /**
   * @brief Push an item to the back of the queue
   * 
   * @param item Item to push
   * @return true if successful, false if queue is full
   */
  bool push(const T& item) {
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
      bool result = false;
      if (queue.size() < max_size) {
        queue.push_back(item);
        result = true;
      }
      xSemaphoreGive(mutex);
      return result;
    }
    return false;
  }

  /**
   * @brief Pop an item from the front of the queue
   * 
   * @param item Reference to store the popped item
   * @return true if successful, false if queue is empty
   */
  bool pop(T& item) {
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
      bool result = false;
      if (!queue.empty()) {
        item = queue.front();
        queue.pop_front();
        result = true;
      }
      xSemaphoreGive(mutex);
      return result;
    }
    return false;
  }

  /**
   * @brief Get the front item without removing it
   * 
   * @param item Reference to store the front item
   * @return true if successful, false if queue is empty
   */
  bool front(T& item) {
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
      bool result = false;
      if (!queue.empty()) {
        item = queue.front();
        result = true;
      }
      xSemaphoreGive(mutex);
      return result;
    }
    return false;
  }

  /**
   * @brief Check if the queue is empty
   * 
   * @return true if empty, false otherwise
   */
  bool empty(void) {
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
      bool result = queue.empty();
      xSemaphoreGive(mutex);
      return result;
    }
    return true; // Assume empty if mutex cannot be acquired
  }

  /**
   * @brief Get the size of the queue
   * 
   * @return size_t Queue size
   */
  size_t size(void) {
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
      size_t result = queue.size();
      xSemaphoreGive(mutex);
      return result;
    }
    return 0; // Assume empty if mutex cannot be acquired
  }

  /**
   * @brief Clear the queue
   */
  void clear(void) {
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
      queue.clear();
      xSemaphoreGive(mutex);
    }
  }

private:
  std::deque<T> queue;         // The underlying queue
  SemaphoreHandle_t mutex;     // Mutex for thread safety
  size_t max_size;             // Maximum size of the queue
};

SINRICPRO_NAMESPACE_END