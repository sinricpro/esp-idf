#include "sinricpro/sinricpro.h"
#include "sinricpro/sinricpro_signature.h"
#include <sstream>

namespace sinricpro {

// Singleton instance
sinricpro_t& SinricPro = sinricpro_t::get_instance();

sinricpro_t& sinricpro_t::get_instance() {
  static sinricpro_t instance;
  return instance;
}

sinricpro_t::sinricpro_t() 
  : websocket(nullptr),
    begin_called(false),
    websocket_task(nullptr) {
  
  // Create event group for synchronization
  event_group = xEventGroupCreate();
}

sinricpro_t::~sinricpro_t() {
  stop();
  
  // Clean up event group
  if (event_group) {
    vEventGroupDelete(event_group);
    event_group = nullptr;
  }
  
  // Clean up devices
  for (auto device : devices) {
    delete device;
  }
  devices.clear();
}

bool sinricpro_t::begin(const std::string& app_key, const std::string& app_secret, const std::string& server_url) {
  SINRICPRO_FUNCTION_BEGIN(TAG);
  
  // Store credentials
  this->app_key = app_key;
  this->app_secret = app_secret;
  this->server_url = server_url;
  
  // Check for valid credentials
  if (app_key.empty() || app_secret.empty()) {
    ESP_LOGE(TAG, "App key or app secret is empty");
    return false;
  }
  
  // Create WebSocket client
  if (!websocket) {
    websocket = new sinricpro_websocket_t();
    
    // Set callbacks
    websocket->on_message([this](const std::string& message) {
      receive_queue.push(message);
      xEventGroupSetBits(event_group, SINRICPRO_EVENT_MESSAGE_RECEIVED);
    });
    
    websocket->on_connected([this]() {
      ESP_LOGI(TAG, "Connected to SinricPro server");
      xEventGroupSetBits(event_group, SINRICPRO_EVENT_CONNECTED);
      if (on_connected_cb) on_connected_cb();
    });
    
    websocket->on_disconnected([this]() {
      ESP_LOGI(TAG, "Disconnected from SinricPro server");
      xEventGroupSetBits(event_group, SINRICPRO_EVENT_DISCONNECTED);
      if (on_disconnected_cb) on_disconnected_cb();
    });
  }
 
  // Collect device IDs
  std::stringstream device_ids;
  for (size_t i = 0; i < devices.size(); i++) {
    device_ids << devices[i]->get_device_id();
    if (i < devices.size() - 1) device_ids << ",";
  }
  
  // Start WebSocket connection
  if (!websocket->begin(app_key, app_secret, server_url, device_ids.str(), true)) {
    ESP_LOGE(TAG, "Failed to initialize WebSocket connection");
    return false;
  }
   
  // Start WebSocket task
  start_websocket_task();
    
  begin_called = true;
  
  SINRICPRO_FUNCTION_END(TAG);
  return true;
}

void sinricpro_t::stop() {
  SINRICPRO_FUNCTION_BEGIN(TAG);
  
  if (!begin_called) {
    return;
  }
  
  // Signal tasks to stop
  xEventGroupSetBits(event_group, SINRICPRO_EVENT_SHUTDOWN);
  
  // Wait for tasks to stop
  if (websocket_task) {
    vTaskDelay(pdMS_TO_TICKS(100));
    vTaskDelete(websocket_task);
    websocket_task = nullptr;
  } 
  
  // Stop WebSocket
  if (websocket) {
    websocket->stop();
    delete websocket;
    websocket = nullptr;
  }
  
  // Clear queues
  std::string tmp;
  while (receive_queue.pop(tmp)) {}
  while (send_queue.pop(tmp)) {}
  
  begin_called = false;
  
  SINRICPRO_FUNCTION_END(TAG);
}

bool sinricpro_t::is_connected() const {
  return websocket && websocket->is_connected();
}

void sinricpro_t::set_connected_callback(connected_callback_t cb) {
  on_connected_cb = cb;
}

void sinricpro_t::set_disconnected_callback(disconnected_callback_t cb) {
  on_disconnected_cb = cb;
}

sinricpro_device_t* sinricpro_t::get_device(const std::string& device_id) {
  for (auto device : devices) {
    if (device->get_device_id() == device_id) {
      return device;
    }
  }
  return nullptr;
}

void sinricpro_t::handle() {
  if (!begin_called) return;
  
  // Handle receive queue
  handle_receive_queue();
  
  // Handle send queue
  handle_send_queue();
}

bool sinricpro_t::handle_receive_queue() {
  std::string message;
  bool handled = false;
  
  while (receive_queue.pop(message)) {
    ESP_LOGD(TAG, "Received message: %s", message.c_str());
    
    // Parse message
    cJSON* json = cJSON_Parse(message.c_str());
    if (!json) {
      ESP_LOGE(TAG, "Failed to parse message: %s", message.c_str());
      continue;
    }
    
    // Get device ID
    cJSON* device_id_json = cJSON_GetObjectItem(json, "deviceId");
    if (!device_id_json || !cJSON_IsString(device_id_json)) {
      ESP_LOGE(TAG, "Message missing deviceId: %s", message.c_str());
      cJSON_Delete(json);
      continue;
    }
    
    std::string device_id = cJSON_GetStringValue(device_id_json);
    
    // Find device
    sinricpro_device_t* device = get_device(device_id);
    if (!device) {
      ESP_LOGE(TAG, "Device not found: %s", device_id.c_str());
      cJSON_Delete(json);
      continue;
    }
    
    // Handle request
    std::string response = device->handle_request(message);
    
    // Send response if not empty
    if (!response.empty()) {
      send_queue.push(response);
      handled = true;
    }
    
    cJSON_Delete(json);
  }
  
  return handled;
}

bool sinricpro_t::handle_send_queue() {
  std::string message;
  bool handled = false;
  
  while (send_queue.pop(message)) {
    ESP_LOGD(TAG, "Sending message: %s", message.c_str());
    
    if (websocket && websocket->is_connected()) {
      if (websocket->send_message(message)) {
        handled = true;
      } else {
        ESP_LOGE(TAG, "Failed to send message");
      }
    } else {
      ESP_LOGW(TAG, "WebSocket not connected, message not sent");
    }
  }
  
  return handled;
}

void sinricpro_t::start_websocket_task() {
  xTaskCreate(
    websocket_task_handler,
    "sinricpro_ws",
    SINRICPRO_TASK_STACK_SIZE,
    this,
    SINRICPRO_TASK_PRIORITY,
    &websocket_task
  );
} 

void sinricpro_t::websocket_task_handler(void* pvParameters) {
  sinricpro_t* sinric = static_cast<sinricpro_t*>(pvParameters);
  
  while (true) {
    // Check for shutdown signal
    EventBits_t bits = xEventGroupGetBits(sinric->event_group);
    if (bits & SINRICPRO_EVENT_SHUTDOWN) {
      ESP_LOGI(sinric->TAG, "WebSocket task shutting down");
      break;
    }
    
    // Handle WebSocket events
    if (sinric->websocket) {
      sinric->websocket->loop();
    }
    
    // Handle receive queue
    sinric->handle_receive_queue();
    
    // Handle send queue
    sinric->handle_send_queue();
    
    // Small delay to prevent hogging CPU
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  
  vTaskDelete(nullptr);
}

bool sinricpro_t::send_message(const std::string& message) {
  if (!is_connected()) {
    ESP_LOGW(TAG, "Not connected to server, message not sent");
    return false;
  }
  
  send_queue.push(message);
  return true;
}

} // namespace sinricpro