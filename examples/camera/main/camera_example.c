/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 *
 * SinricPro camera with live view in the SinricPro portal and app. Video is JPEG
 * over a WebRTC DataChannel; signaling runs through SinricPro (getWebRTCAnswer),
 * and STUN/TURN servers arrive with each offer, so viewing works outside the LAN.
 * Viewers can change resolution and frame rate and toggle flash, flip and mirror.
 *
 * Portal setup: device type Camera -> Camera Stream Configuration: Board "ESP32",
 * Streaming Protocol "WebRTC". Alexa and Google Home streaming is not supported.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_camera.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "sinricpro.h"
#include "sinricpro_camera.h"
#include "webrtc_camera.h"
#include "camera_boards.h"

#ifdef CONFIG_CAMERA_MICROPHONE
#include "driver/i2s_pdm.h"
#endif

/* WiFi Configuration - MODIFY THESE */
#define WIFI_SSID      "WIFI_SSID"
#define WIFI_PASS      "WIFI_PASS"

/* SinricPro Configuration - MODIFY THESE */
#define DEVICE_ID      "DEVICE_ID"      /* 24-character hex string */
#define APP_KEY        "APP_KEY"        /* From SinricPro portal */
#define APP_SECRET     "APP_SECRET"     /* From SinricPro portal */

/* The server sends one STUN URL and a few TURN URLs; the session uses at most 8. */
#define MAX_ICE_SERVERS 8

static const char *TAG = "camera_example";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static webrtc_camera_handle_t s_session = NULL;
static sinricpro_device_handle_t s_camera = NULL;

/* ===========================================================================
 * WiFi
 * =========================================================================== */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, retrying...");
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* Modem sleep delays every outgoing packet, which a video stream cannot absorb. */
    esp_wifi_set_ps(WIFI_PS_NONE);

    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        ESP_LOGI(TAG, "Connected to %s, RSSI %d dBm", WIFI_SSID, ap.rssi);
        if (ap.rssi < -75) {
            ESP_LOGW(TAG, "Live view needs about -75 dBm or better; move the camera closer to the access point");
        }
    }
}

/* ===========================================================================
 * Camera and microphone
 * =========================================================================== */

static esp_err_t camera_init(void)
{
    if (!esp_psram_is_initialized()) {
        ESP_LOGE(TAG, "PSRAM is not available; camera frame buffers and WebRTC need it");
        return ESP_ERR_NO_MEM;
    }

    camera_board_prepare();
    camera_config_t config = camera_board_config();
    /* Frame buffers are sized for the init resolution, so initialise at the largest
     * size viewers may pick and start streaming smaller. */
    config.frame_size = FRAMESIZE_SVGA;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: 0x%x (%s). Check the board selected in menuconfig.",
                 err, esp_err_to_name(err));
        return err;
    }

    sensor_t *sensor = esp_camera_sensor_get();
    sensor->set_framesize(sensor, FRAMESIZE_QVGA);
    ESP_LOGI(TAG, "Camera: %s, PSRAM: %u bytes", camera_board_name(), (unsigned)esp_psram_get_size());
    return ESP_OK;
}

#ifdef CONFIG_CAMERA_MICROPHONE
static i2s_chan_handle_t s_microphone = NULL;

static esp_err_t microphone_init(void)
{
    i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t err = i2s_new_channel(&channel_config, NULL, &s_microphone);
    if (err != ESP_OK) {
        return err;
    }

    /* XIAO ESP32S3 Sense: PDM clock on GPIO42, data on GPIO41. */
    i2s_pdm_rx_config_t pdm_config = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = GPIO_NUM_42,
            .din = GPIO_NUM_41,
        },
    };
    err = i2s_channel_init_pdm_rx_mode(s_microphone, &pdm_config);
    return err == ESP_OK ? i2s_channel_enable(s_microphone) : err;
}

static uint8_t encode_mulaw(int16_t pcm)
{
    int value = pcm;
    int sign = value < 0 ? 0x80 : 0;
    if (value < 0) {
        value = -value;
    }
    if (value > 32635) {
        value = 32635;
    }
    value += 0x84;
    int exponent = 7;
    for (int mask = 0x4000; exponent > 0 && !(value & mask); mask >>= 1) {
        exponent--;
    }
    return (uint8_t)~(sign | (exponent << 4) | ((value >> (exponent + 3)) & 0x0F));
}

/* 20 ms of 16 kHz PDM audio, averaged down to the 8 kHz PCMU the WebRTC audio track carries. */
static bool read_microphone(uint8_t *pcmu, size_t size, void *ctx)
{
    static int16_t pcm[320];
    static size_t used = 0;

    size_t bytes = 0;
    i2s_channel_read(s_microphone, (uint8_t *)pcm + used, sizeof(pcm) - used, &bytes, 0);
    used += bytes;
    if (used < sizeof(pcm)) {
        return false;
    }
    used = 0;

    for (size_t i = 0; i < size && 2 * i + 1 < 320; i++) {
        pcmu[i] = encode_mulaw((int16_t)(((int32_t)pcm[2 * i] + pcm[2 * i + 1]) / 2));
    }
    return true;
}
#endif

/* ===========================================================================
 * SinricPro callbacks
 * =========================================================================== */

static bool on_webrtc_offer(const char *device_id, const char *offer_sdp,
                            const sinricpro_ice_server_t *ice_servers, size_t ice_server_count,
                            char **answer_sdp, void *user_data)
{
    /* Field for field the same; the SDK and the session component stay independent of each other. */
    webrtc_camera_ice_server_t servers[MAX_ICE_SERVERS];
    size_t count = ice_server_count < MAX_ICE_SERVERS ? ice_server_count : MAX_ICE_SERVERS;
    for (size_t i = 0; i < count; i++) {
        servers[i] = (webrtc_camera_ice_server_t){
            .url = ice_servers[i].url,
            .username = ice_servers[i].username,
            .credential = ice_servers[i].credential,
        };
    }

    ESP_LOGI(TAG, "WebRTC offer for %s with %u ICE server URLs", device_id, (unsigned)count);

    char error[160];
    esp_err_t err = webrtc_camera_handle_offer(s_session, offer_sdp, servers, count,
                                               answer_sdp, error, sizeof(error));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "WebRTC answer failed: %s", error);
        /* Shown to the viewer in the SinricPro app and portal. */
        sinricpro_set_response_message(error);
        return false;
    }

    ESP_LOGI(TAG, "WebRTC answer sent");
    return true;
}

static bool on_power_state(const char *device_id, bool *state, void *user_data)
{
    ESP_LOGI(TAG, "PowerState: %s", *state ? "ON" : "OFF");
    if (!*state) {
        webrtc_camera_stop_viewer(s_session);
    }
    return true;
}

static void sinricpro_event_handler(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data)
{
    if (event_id == SINRICPRO_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "Connected to SinricPro server");
    } else if (event_id == SINRICPRO_EVENT_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected from SinricPro server");
    }
}

/* Internal RAM is the pool that runs out first: Wi-Fi, TLS and DTLS draw on it where PSRAM
 * cannot substitute. A healthy total beside a small largest block is fragmentation rather
 * than exhaustion, and the two need different fixes. */
static void log_stats(void)
{
    wifi_ap_record_t ap;
    int rssi = esp_wifi_sta_get_ap_info(&ap) == ESP_OK ? ap.rssi : 0;

    ESP_LOGI(TAG, "Heap free %u (min %u), internal %u (largest %u), PSRAM free %u, RSSI %d, streaming: %s",
             (unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size(),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM), rssi,
             webrtc_camera_is_streaming(s_session) ? "yes" : "no");
}

/* ===========================================================================
 * Main Application
 * =========================================================================== */

void app_main(void)
{
    ESP_LOGI(TAG, "SinricPro ESP-IDF Camera Example, SDK %s", sinricpro_get_version());

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (camera_init() != ESP_OK) {
        return;
    }

#ifdef CONFIG_CAMERA_MICROPHONE
    if (microphone_init() != ESP_OK) {
        ESP_LOGE(TAG, "Microphone initialization failed");
        return;
    }
#endif

    wifi_init_sta();

    webrtc_camera_config_t session_config = WEBRTC_CAMERA_CONFIG_DEFAULT();
    session_config.max_frame_size = FRAMESIZE_SVGA;  /* the size camera_init() allocated for */
    session_config.flash_gpio = CONFIG_CAMERA_FLASH_GPIO;
#if CONFIG_IDF_TARGET_ESP32
    /* Once Wi-Fi and the SinricPro TLS socket are up, classic ESP32 has little contiguous
     * internal RAM left. Caches large enough to consume it leave the Wi-Fi driver unable to
     * allocate TX buffers, and the DTLS handshake never completes. */
    session_config.data_channel_send_cache = 6 * 1024;
    session_config.data_channel_recv_cache = 3 * 1024;
#endif
#ifdef CONFIG_CAMERA_MICROPHONE
    session_config.audio_source = read_microphone;
#endif

    ret = webrtc_camera_start(&session_config, &s_session);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start the WebRTC session: %s", esp_err_to_name(ret));
        return;
    }

    ESP_ERROR_CHECK(esp_event_handler_register(SINRICPRO_EVENT, ESP_EVENT_ANY_ID,
                                               &sinricpro_event_handler, NULL));

    sinricpro_config_t sinric_config = {
        .app_key = APP_KEY,
        .app_secret = APP_SECRET,
        .auto_reconnect = true,
        .reconnect_interval_ms = 5000,
        .heartbeat_interval_ms = 0,
    };
    ret = sinricpro_init(&sinric_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SinricPro: %s", esp_err_to_name(ret));
        return;
    }

    s_camera = sinricpro_camera_create(DEVICE_ID);
    if (s_camera == NULL) {
        ESP_LOGE(TAG, "Failed to create camera device");
        return;
    }
    sinricpro_camera_on_power_state(s_camera, on_power_state, NULL);
    sinricpro_camera_on_webrtc_offer(s_camera, on_webrtc_offer, NULL);
#ifdef CONFIG_CAMERA_MICROPHONE
    /* Viewers request an audio track only when this is set. */
    sinricpro_camera_enable_webrtc_audio(s_camera, true);
#endif

    ret = sinricpro_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start SinricPro: %s", esp_err_to_name(ret));
        return;
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        log_stats();
    }
}
