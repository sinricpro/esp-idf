#include "sinricpro/sinricpro_device.h"
#include "sinricpro/sinricpro.h"
#include "esp_log.h"
#include <sstream>

namespace sinricpro {

// Request implementation
std::string sinricpro_request_t::get_device_id(void) const {
  cJSON* device_id = cJSON_GetObjectItem(json, "deviceId");
  if (device_id && cJSON_IsString(device_id)) {
    return cJSON_GetStringValue(device_id);
  }
  return "";
}

std::string sinricpro_request_t::get_action(void) const {
  cJSON* action = cJSON_GetObjectItem(json, "action");
  if (action && cJSON_IsString(action)) {
    return cJSON_GetStringValue(action);
  }
  return "";
}

cJSON* sinricpro_request_t::get_value(const char* key) const {
  cJSON* payload = cJSON_GetObjectItem(json, "payload");
  if (!payload) return nullptr;
  
  cJSON* value = cJSON_GetObjectItem(payload, "value");
  if (!value) return nullptr;
  
  return cJSON_GetObjectItem(value, key);
}

// Response implementation
sinricpro_response_t::sinricpro_response_t(const sinricpro_request_t& request) {
  // Create a new response JSON
  json = cJSON_CreateObject();
  
  // Copy necessary fields from request
  cJSON* req_header = cJSON_GetObjectItem(request.json, "header");
  if (req_header) {
    cJSON* header = cJSON_CreateObject();
    
    // Copy message ID
    cJSON* message_id = cJSON_GetObjectItem(req_header, "messageId");
    if (message_id) cJSON_AddItemToObject(header, "messageId", cJSON_Duplicate(message_id, true));
    
    // Copy correlation ID
    cJSON* correlation_id = cJSON_GetObjectItem(req_header, "correlationId");
    if (correlation_id) cJSON_AddItemToObject(header, "correlationId", cJSON_Duplicate(correlation_id, true));
    
    // Set name to Response
    cJSON_AddStringToObject(header, "name", "Response");
    
    // Copy namespace
    cJSON* ns = cJSON_GetObjectItem(req_header, "namespace");
    if (ns) cJSON_AddItemToObject(header, "namespace", cJSON_Duplicate(ns, true));
    
    // Add header to response
    cJSON_AddItemToObject(json, "header", header);
  }
  
  // Copy device ID
  cJSON* device_id = cJSON_GetObjectItem(request.json, "deviceId");
  if (device_id) cJSON_AddItemToObject(json, "deviceId", cJSON_Duplicate(device_id, true));
  
  // Create payload
  cJSON* payload = cJSON_CreateObject();
  cJSON_AddItemToObject(json, "payload", payload);
  
  // Add default success status
  set_success(true);
}

sinricpro_response_t::~sinricpro_response_t() {
  if (json) cJSON_Delete(json);
}

void sinricpro_response_t::set_value(const char* key, bool value) {
  cJSON* payload = cJSON_GetObjectItem(json, "payload");
  if (!payload) return;
  
  cJSON* values = cJSON_GetObjectItem(payload, "value");
  if (!values) {
    values = cJSON_CreateObject();
    cJSON_AddItemToObject(payload, "value", values);
  }
  
  cJSON_AddBoolToObject(values, key, value);
}

void sinricpro_response_t::set_value(const char* key, int value) {
  cJSON* payload = cJSON_GetObjectItem(json, "payload");
  if (!payload) return;
  
  cJSON* values = cJSON_GetObjectItem(payload, "value");
  if (!values) {
    values = cJSON_CreateObject();
    cJSON_AddItemToObject(payload, "value", values);
  }
  
  cJSON_AddNumberToObject(values, key, value);
}

void sinricpro_response_t::set_value(const char* key, float value) {
  cJSON* payload = cJSON_GetObjectItem(json, "payload");
  if (!payload) return;
  
  cJSON* values = cJSON_GetObjectItem(payload, "value");
  if (!values) {
    values = cJSON_CreateObject();
    cJSON_AddItemToObject(payload, "value", values);
  }
  
  cJSON_AddNumberToObject(values, key, value);
}

void sinricpro_response_t::set_value(const char* key, const char* value) {
  cJSON* payload = cJSON_GetObjectItem(json, "payload");
  if (!payload) return;
  
  cJSON* values = cJSON_GetObjectItem(payload, "value");
  if (!values) {
    values = cJSON_CreateObject();
    cJSON_AddItemToObject(payload, "value", values);
  }
  
  cJSON_AddStringToObject(values, key, value);
}

void sinricpro_response_t::set_value(const char* key, const std::string& value) {
  set_value(key, value.c_str());
}

void sinricpro_response_t::set_success(bool success) {
  cJSON* payload = cJSON_GetObjectItem(json, "payload");
  if (!payload) return;
  
  cJSON_AddBoolToObject(payload, "success", success);
}

std::string sinricpro_response_t::to_string() const {
  char* json_str = cJSON_PrintUnformatted(json);
  std::string result = json_str;
  free(json_str);
  return result;
}

// Device implementation
sinricpro_device_t::sinricpro_device_t(const std::string& device_id) 
  : device_id(device_id), 
    sinric(nullptr) {
  
  request_handlers_mutex = xSemaphoreCreateMutex();
}

sinricpro_device_t::~sinricpro_device_t() {
  if (request_handlers_mutex) {
    vSemaphoreDelete(request_handlers_mutex);
  }
}

void sinricpro_device_t::begin(sinricpro_t* sinric) {
  this->sinric = sinric;
}

std::string sinricpro_device_t::get_device_id() const {
  return device_id;
}

void sinricpro_device_t::add_request_handler(const std::string& action, request_handler_t handler) {
  if (xSemaphoreTake(request_handlers_mutex, portMAX_DELAY) == pdTRUE) {
    request_handlers[action] = handler;
    xSemaphoreGive(request_handlers_mutex);
  }
}

std::string sinricpro_device_t::handle_request(const std::string& request_string) {
  cJSON* json = cJSON_Parse(request_string.c_str());
  if (!json) {
    ESP_LOGE("SinricDevice", "Failed to parse request JSON");
    return "";
  }
  
  // Create request object
  sinricpro_request_t request(json);
  std::string action = request.get_action();
  
  // Find handler for action
  request_handler_t handler = nullptr;
  
  if (xSemaphoreTake(request_handlers_mutex, portMAX_DELAY) == pdTRUE) {
    auto it = request_handlers.find(action);
    if (it != request_handlers.end()) {
      handler = it->second;
    }
    xSemaphoreGive(request_handlers_mutex);
  }
  
  // Handle request
  if (handler) {
    // Create response object
    sinricpro_response_t response(request);
    
    // Call handler
    bool handled = handler(request);
    
    // Set success status based on handler result
    response.set_success(handled);
    
    // Return response string
    return response.to_string();
  } else {
    ESP_LOGW("SinricDevice", "No handler found for action: %s", action.c_str());
  }
  
  cJSON_Delete(json);
  return "";
}

bool sinricpro_device_t::send_event(const std::string& event_name, const std::string& cause) {
  if (!sinric || !sinric->is_connected()) {
    return false;
  }
  
  // Create event JSON
  cJSON* json = cJSON_CreateObject();
  
  // Add header
  cJSON* header = cJSON_CreateObject();
  cJSON_AddStringToObject(header, "payloadVersion", "2");
  cJSON_AddStringToObject(header, "namespace", "Alexa");
  cJSON_AddStringToObject(header, "name", event_name.c_str());
  cJSON_AddItemToObject(json, "header", header);
  
  // Add device ID
  cJSON_AddStringToObject(json, "deviceId", device_id.c_str());
  
  // Add payload
  cJSON* payload = cJSON_CreateObject();
  cJSON_AddStringToObject(payload, "cause", cause.c_str());
  cJSON_AddItemToObject(json, "payload", payload);
  
  // Convert to string
  char* json_str = cJSON_PrintUnformatted(json);
  std::string message = json_str;
  free(json_str);
  
  // Send event
  bool result = sinric->send_message(message);
  
  cJSON_Delete(json);
  return result;
}

} // namespace sinricpro