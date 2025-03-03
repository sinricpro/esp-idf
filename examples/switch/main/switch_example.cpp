#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "sinricpro.h"

using namespace sinricpro;

// SinricPro credentials
#define APP_KEY       CONFIG_SINRICPRO_APP_KEY       // From Kconfig
#define APP_SECRET    CONFIG_SINRICPRO_APP_SECRET    // From Kconfig
#define SWITCH_ID     CONFIG_SINRICPRO_SWITCH_ID     // From Kconfig

// WiFi credentials
#define WIFI_SSID     CONFIG_WIFI_SSID               // From Kconfig
#define WIFI_PASSWORD CONFIG_WIFI_PASSWORD           // From Kconfig

// GPIO configuration
#define LED_PIN       GPIO_NUM_2                     // GPIO connected to LED (built-in LED on most ESP32 boards)
#define BUTTON_PIN    GPIO_NUM_0                     // GPIO connected to button (built-in button on most ESP32 boards)

// Event group for WiFi events
static EventGroupHandle_t s_wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;

// Button state
static bool s_button_pressed = false;
static bool s_last_button_state = false;
static int64_t s_last_button_time = 0;

// State variables
static bool s_switch_state = false;

static const char *TAG = "sinricpro_switch";

// Function prototypes
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void initialize_wifi(void);
static void button_task(void *pvParameters);

// SinricPro callbacks
bool on_power_state(const std::string& device_id, bool state) {
    ESP_LOGI(TAG, "Device %s power state changed to %s", device_id.c_str(), state ? "ON" : "OFF");
    
    // Update the global state
    s_switch_state = state;
    
    // Control the LED
    gpio_set_level(LED_PIN, state ? 1 : 0);
    
    return true;
}

void sinricpro_connected_cb(void) {
    ESP_LOGI(TAG, "Connected to SinricPro");
}

void sinricpro_disconnected_cb(void) {
    ESP_LOGI(TAG, "Disconnected from SinricPro");
}

extern "C" void app_main(void) {
    // Initialize NVS flash
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize WiFi
    initialize_wifi();
    
    // Initialize GPIO
    gpio_config_t io_conf = {};
    // LED configuration (output)
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LED_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    // Button configuration (input)
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BUTTON_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    
    // Initial LED state (off)
    gpio_set_level(LED_PIN, 0);
    
    // Start button task
    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);
    
    // Initialize timestamp
    initialize_timestamp();
    
    // Set up SinricPro
    sinricpro_switch_t& my_switch = SinricPro[SWITCH_ID];
    my_switch.set_power_state_callback(on_power_state);
    
    // Set callbacks
    SinricPro.set_connected_callback(sinricpro_connected_cb);
    SinricPro.set_disconnected_callback(sinricpro_disconnected_cb);
    
    // Start SinricPro
    if (!SinricPro.begin(APP_KEY, APP_SECRET)) {
        ESP_LOGE(TAG, "Failed to start SinricPro");
        return;
    }
    
    ESP_LOGI(TAG, "SinricPro initialized");
    
    // Main loop is handled by SinricPro tasks, we don't need to do anything here
    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, trying to reconnect...");
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "WiFi connected, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void initialize_wifi(void) {
    s_wifi_event_group = xEventGroupCreate();
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password));
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi initialization completed");
    
    // Wait for WiFi connection
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
}

static void button_task(void *pvParameters) {
    sinricpro_switch_t& my_switch = SinricPro[SWITCH_ID];
    
    while (true) {
        // Read button state
        bool button_state = gpio_get_level(BUTTON_PIN) == 0; // Low active
        
        // Simple debouncing
        int64_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
        if (button_state != s_last_button_state && (current_time - s_last_button_time) > 50) {
            // Button state changed and debounce time passed
            s_last_button_time = current_time;
            
            if (button_state) {
                // Button pressed
                s_button_pressed = true;
                
                // Toggle switch state
                s_switch_state = !s_switch_state;
                
                // Update LED
                gpio_set_level(LED_PIN, s_switch_state ? 1 : 0);
                
                // Send event to SinricPro
                my_switch.send_power_state_event(s_switch_state, "PHYSICAL_INTERACTION");
                
                ESP_LOGI(TAG, "Button pressed, switch state changed to %s", s_switch_state ? "ON" : "OFF");
            }
        }
        
        s_last_button_state = button_state;
        
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}