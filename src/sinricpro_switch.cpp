#include "sinricpro/sinricpro_switch.h"
#include "esp_log.h"

namespace sinricpro {

static const char *SWITCH_TAG = "SinricProSwitch";

void sinricpro_switch_t::set_power_state_callback(power_state_cb_t callback) {
  power_state_callback = callback;
  
  // Register action handler
  add_request_handler("setPowerState", [this](sinricpro_request_t& request) {
    return handle_power_state(request);
  });
}

bool sinricpro_switch_t::send_power_state_event(bool state, const std::string& cause) {
  // Create power state event
  std::string event = prepare_event(SINRICPRO_POWERSTATE_ACTION, cause.c_str());
  
  // Add state to the event
  cJSON* json = cJSON_Parse(event.c_str());
  if (!json) {
    ESP_LOGE(SWITCH_TAG, "Failed to parse event JSON");
    return false;
  }
  
  // Add value to payload
  cJSON* payload = cJSON_GetObjectItem(json, "payload");
  if (!payload) {
    ESP_LOGE(SWITCH_TAG, "No payload in event");
    cJSON_Delete(json);
    return false;
  }
  
  cJSON* value = cJSON_GetObjectItem(payload, "value");
  if (!value) {
    ESP_LOGE(SWITCH_TAG, "No value in payload");
    cJSON_Delete(json);
    return false;
  }
  
  // Add state
  cJSON_AddStringToObject(value, SINRICPRO_POWERSTATE_STATE, 
                         state ? SINRICPRO_POWERSTATE_ON : SINRICPRO_POWERSTATE_OFF);
  
  // Convert to string
  char* event_string = cJSON_PrintUnformatted(json);
  std::string result(event_string);
  
  // Clean up
  cJSON_free(event_string);
  cJSON_Delete(json);
  
  // Send the event
  return send_event(result);
}

bool sinricpro_switch_t::handle_power_state(sinricpro_request_t& request) {
  // Get state value
  cJSON* state_json = request.get_value(SINRICPRO_POWERSTATE_STATE);
  if (!state_json || !cJSON_IsString(state_json)) {
    ESP_LOGE(SWITCH_TAG, "No state in request");
    return false;
  }
  
  std::string state_str = cJSON_GetStringValue(state_json);
  bool state = (state_str == SINRICPRO_POWERSTATE_ON);
  
  // Call callback if registered
  if (power_state_callback) {
    return power_state_callback(get_device_id(), state);
  }
  
  return false;
}

} // namespace sinricpro