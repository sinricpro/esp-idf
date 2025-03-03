#include "sinricpro/sinricpro_websocket.h"
#include "sinricpro/sinricpro_signature.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include <sstream>

namespace sinricpro {

sinricpro_websocket_t::sinricpro_websocket_t() 
  : client(nullptr), 
    connected(false) {
  
  // Create event group and mutex
  event_group = xEventGroupCreate();
  mutex = xSemaphoreCreateMutex();
}

sinricpro_websocket_t::~sinricpro_websocket_t() {
  stop();
  
  if (event_group) {
    vEventGroupDelete(event_group);
    event_group = nullptr;
  }
  
  if (mutex) {
    vSemaphoreDelete(mutex);
    mutex = nullptr;
  }
}

bool sinricpro_websocket_t::begin(const std::string& app_key, const std::string& app_secret, 
                               const std::string& server_url, const std::string& device_ids,
                               bool reconnect) {
  // Store credentials
  this->app_key = app_key;
  this->app_secret = app_secret;
  this->server_url = server_url;
  this->device_ids = device_ids;
  this->auto_reconnect = reconnect;
  
  std::string url = build_url();
  
  // Create WebSocket client if needed
  if (!client) {
    client = new esp_websocket_client_t();
    memset(client, 0, sizeof(esp_websocket_client_t));
  }
  
  // Configure WebSocket client
  esp_websocket_client_config_t config = {};
  config.uri = url.c_str();
  
  // Store WebSocket credentials
  client->config = config;
  
  // Initialize WebSocket client
  client->handle = esp_websocket_client_init(&client->config);
  if (!client->handle) {
    ESP_LOGE("SinricWebSocket", "Failed to initialize WebSocket client");
    return false;
  }
  
  // Register event handlers
  esp_websocket_register_events(client->handle, WEBSOCKET_EVENT_ANY, websocket_event_handler, this);
  
  // Start WebSocket connection
  if (esp_websocket_client_start(client->handle) != ESP_OK) {
    ESP_LOGE("SinricWebSocket", "Failed to start WebSocket client");
    return false;
  }
  
  ESP_LOGI("SinricWebSocket", "WebSocket client started");
  return true;
}

void sinricpro_websocket_t::stop() {
  if (!client || !client->handle) return;
  
  // Stop WebSocket client
  if (esp_websocket_client_is_connected(client->handle)) {
    esp_websocket_client_stop(client->handle);
  }
  
  // Destroy WebSocket client
  esp_websocket_client_destroy(client->handle);
  
  client->handle = nullptr;
  
  connected = false;
}

bool sinricpro_websocket_t::is_connected() const {
  return connected && client && client->handle && esp_websocket_client_is_connected(client->handle);
}

void sinricpro_websocket_t::loop() {
  if (!client || !client->handle) return;
  
  // Check if connected
  bool current_connected = esp_websocket_client_is_connected(client->handle);
  
  // Handle connection state changes
  if (current_connected != connected) {
    connected = current_connected;
    
    if (connected) {
      // Connected - call callback
      if (on_connected_callback) {
        on_connected_callback();
      }
    } else {
      // Disconnected - call callback
      if (on_disconnected_callback) {
        on_disconnected_callback();
      }
      
      // Auto-reconnect if enabled
      if (auto_reconnect) {
        ESP_LOGI("SinricWebSocket", "Auto-reconnecting...");
        begin(app_key, app_secret, server_url, device_ids, auto_reconnect);
      }
    }
  }
}

bool sinricpro_websocket_t::send_message(const std::string& message) {
  if (!is_connected()) return false;
  
  int result = esp_websocket_client_send_text(client->handle, message.c_str(), message.length(), portMAX_DELAY);
  return (result >= 0);
}

void sinricpro_websocket_t::on_message(message_callback_t callback) {
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    on_message_callback = callback;
    xSemaphoreGive(mutex);
  }
}

void sinricpro_websocket_t::on_connected(connected_callback_t callback) {
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    on_connected_callback = callback;
    xSemaphoreGive(mutex);
  }
}

void sinricpro_websocket_t::on_disconnected(disconnected_callback_t callback) {
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    on_disconnected_callback = callback;
    xSemaphoreGive(mutex);
  }
}

std::string sinricpro_websocket_t::build_url() {
  std::stringstream url;
  
  // Build URL with query parameters
  url << server_url
      << "?appkey=" << app_key
      << "&deviceids=" << device_ids;
  
  // Add client info
  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  
  char mac_str[18];
  snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  url << "&platform=ESP-IDF"
      << "&version=" << SINRICPRO_VERSION
      << "&mac=" << mac_str;
  
  // Add signature
  std::string timestamp = get_timestamp();
  url << "&timestamp=" << timestamp
      << "&signature=" << create_signature(app_key, app_secret, timestamp);
  
  return url.str();
}

void sinricpro_websocket_t::websocket_event_handler(void* arg, esp_event_base_t base, int32_t event_id, void* event_data) {
  sinricpro_websocket_t* ws = static_cast<sinricpro_websocket_t*>(arg);
  esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;
  
  switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
      ESP_LOGI("SinricWebSocket", "WebSocket connected");
      ws->connected = true;
      xEventGroupSetBits(ws->event_group, SINRICPRO_WS_CONNECTED);
      break;
      
    case WEBSOCKET_EVENT_DISCONNECTED:
      ESP_LOGI("SinricWebSocket", "WebSocket disconnected");
      ws->connected = false;
      xEventGroupSetBits(ws->event_group, SINRICPRO_WS_DISCONNECTED);
      break;
      
    case WEBSOCKET_EVENT_DATA:
      // Process received data
      if (data->op_code == WS_TRANSPORT_OPCODES_TEXT && data->data_len > 0) {
        // Call the message callback if registered
        if (xSemaphoreTake(ws->mutex, portMAX_DELAY) == pdTRUE) {
          if (ws->on_message_callback) {
            std::string message(reinterpret_cast<const char*>(data->data_ptr), data->data_len);
            ws->on_message_callback(message);
          }
          xSemaphoreGive(ws->mutex);
        }
      }
      break;
      
    case WEBSOCKET_EVENT_ERROR:
      ESP_LOGE("SinricWebSocket", "WebSocket error occurred");
      break;
      
    default:
      break;
  }
}

} // namespace sinricpro