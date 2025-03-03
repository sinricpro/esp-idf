#include "sinricpro/timestamp.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include <time.h>
#include <sys/time.h>
#include <string>
#include <sstream>
#include <iomanip>

namespace sinricpro {

static const char* TAG = "SinricProTime";
static bool time_initialized = false;

// SNTP time sync callback
static void time_sync_notification_cb(struct timeval *tv) {
  ESP_LOGI(TAG, "Time synchronized with SNTP server");
  time_initialized = true;
}

void initialize_timestamp(const char* time_zone) {
  ESP_LOGI(TAG, "Initializing SNTP");
  
  // Set timezone
  setenv("TZ", time_zone, 1);
  tzset();
  
  // Initialize SNTP
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
  esp_sntp_init();
}

std::string get_iso_timestamp() {
  time_t now;
  struct tm timeinfo;
  char timestamp[64];
  
  time(&now);
  localtime_r(&now, &timeinfo);
  
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S.000Z", &timeinfo);
  
  return std::string(timestamp);
}

uint64_t get_timestamp_ms() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  
  return (uint64_t)(tv.tv_sec) * 1000 + (tv.tv_usec / 1000);
}

uint64_t timestamp_to_ms(const std::string& timestamp) {
  struct tm tm = {};
  uint64_t ms = 0;
  
  // Parse ISO8601 timestamp (YYYY-MM-DDTHH:MM:SS.mmmZ)
  if (sscanf(timestamp.c_str(), "%d-%d-%dT%d:%d:%d.%ldZ", 
             &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
             &tm.tm_hour, &tm.tm_min, &tm.tm_sec, 
             &ms) >= 6) {
    
    // Adjust values
    tm.tm_year -= 1900;  // Years since 1900
    tm.tm_mon -= 1;      // Months are 0-11
    
    // Convert to time_t
    time_t time = mktime(&tm);
    
    // Convert to milliseconds
    return (uint64_t)time * 1000 + ms;
  }
  
  ESP_LOGE(TAG, "Failed to parse timestamp: %s", timestamp.c_str());
  return 0;
}

} // namespace sinricpro