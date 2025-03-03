#include "sinricpro/sinricpro_signature.h"
#include "esp_log.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include "mbedtls/md.h"
#include "esp_system.h"
#include "esp_sntp.h"
#include <time.h>
#include <string.h>
#include <sstream>
#include <iomanip>
#include <exception>

namespace sinricpro {

// Logging tag
static const char* TAG = "SinricProSig";

std::string create_signature(const std::string& app_key, const std::string& app_secret, const std::string& message) {
  // Create HMAC-SHA256 signature
  unsigned char hmac[32]; // SHA256 produces 32 bytes
  size_t hmac_len = 0;
  
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1); // 1 for HMAC
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)app_secret.c_str(), app_secret.length());
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)message.c_str(), message.length());
  mbedtls_md_hmac_finish(&ctx, hmac);
  
  // Clean up
  mbedtls_md_free(&ctx);
  
  // Convert HMAC to Base64
  unsigned char base64_output[64]; // Base64 encoded output
  size_t base64_len;
  
  mbedtls_base64_encode(base64_output, sizeof(base64_output), &base64_len, hmac, 32);
  
  // Return as string
  return std::string((char*)base64_output, base64_len);
}

std::string get_timestamp() {
  // Get current time
  time_t now;
  struct tm timeinfo;
  char timestamp[64];
  
  time(&now);
  gmtime_r(&now, &timeinfo);
  
  // Format as ISO8601
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  
  return std::string(timestamp);
}

bool verify_signature(const std::string& payload, const std::string& signature, const std::string& app_secret) {
  // Parse payload
  cJSON* json = cJSON_Parse(payload.c_str());
  if (!json) {
    ESP_LOGE(TAG, "Failed to parse JSON payload");
    return false;
  }
  
  // Get timestamp
  cJSON* timestamp_json = cJSON_GetObjectItem(json, "timestamp");
  if (!timestamp_json || !cJSON_IsString(timestamp_json)) {
    ESP_LOGE(TAG, "Missing timestamp in payload");
    cJSON_Delete(json);
    return false;
  }
  
  // Calculate signature
  std::string calculated_signature = create_signature("", app_secret, payload);
  
  // Compare signatures
  bool result = (calculated_signature == signature);
  
  cJSON_Delete(json);
  return result;
}

} // namespace sinricpro