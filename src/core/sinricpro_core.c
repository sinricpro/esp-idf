/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro.h"
#include "sinricpro_device_internal.h"
#include "sinricpro_websocket.h"
#include "sinricpro_signature.h"
#include "sinricpro_message_queue.h"
#include "sinricpro_udp.h"
#include "sinricpro_mdns.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_random.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "sinricpro_core";

ESP_EVENT_DEFINE_BASE(SINRICPRO_EVENT);

/**
 * @brief Core state structure
 */
static struct {
    sinricpro_config_t config;
    sinricpro_device_t *devices;  /* Linked list of devices */
    size_t device_count;
    sinricpro_message_queue_handle_t send_queue;
    uint32_t timestamp;
    bool initialized;
    bool started;
    SemaphoreHandle_t mutex;
    /* Serialises request dispatch. The websocket task and the local control
     * task both land in the same device callbacks, which were written for a
     * single caller. */
    SemaphoreHandle_t dispatch_mutex;
    TaskHandle_t send_task;
} core_state = {0};

/* Forward declarations */
static void handle_received_message(const char *data, size_t length, void *context);
static void handle_connected(void *context);
static void handle_disconnected(void *context);
static void send_task_func(void *arg);
static void process_incoming(const char *data, size_t length,
                             const sinricpro_msg_origin_t *origin);
static size_t build_device_ids(char *out, size_t out_len, char separator);
static void announce_devices(void);

/* ========================================================================
 * Device Management
 * ======================================================================== */

esp_err_t sinricpro_core_register_device(sinricpro_device_t *device)
{
    if (device == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(core_state.mutex, portMAX_DELAY);

    /* Check if device already registered */
    sinricpro_device_t *d = core_state.devices;
    while (d != NULL) {
        if (strcmp(d->device_id, device->device_id) == 0) {
            xSemaphoreGive(core_state.mutex);
            ESP_LOGW(TAG, "Device already registered: %s", device->device_id);
            return ESP_ERR_INVALID_STATE;
        }
        d = d->next;
    }

    /* Check max devices */
    if (core_state.device_count >= CONFIG_SINRICPRO_MAX_DEVICES) {
        xSemaphoreGive(core_state.mutex);
        ESP_LOGE(TAG, "Maximum number of devices reached (%d)", CONFIG_SINRICPRO_MAX_DEVICES);
        return ESP_ERR_NO_MEM;
    }

    /* Add to linked list */
    device->next = core_state.devices;
    core_state.devices = device;
    core_state.device_count++;

    xSemaphoreGive(core_state.mutex);

    ESP_LOGI(TAG, "Device registered: %s (total: %d)", device->device_id, core_state.device_count);

    announce_devices();

    return ESP_OK;
}

esp_err_t sinricpro_core_unregister_device(const char *device_id)
{
    if (device_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(core_state.mutex, portMAX_DELAY);

    sinricpro_device_t *prev = NULL;
    sinricpro_device_t *curr = core_state.devices;

    while (curr != NULL) {
        if (strcmp(curr->device_id, device_id) == 0) {
            /* Found device, remove from list */
            if (prev == NULL) {
                core_state.devices = curr->next;
            } else {
                prev->next = curr->next;
            }
            core_state.device_count--;

            xSemaphoreGive(core_state.mutex);

            ESP_LOGI(TAG, "Device unregistered: %s (remaining: %d)",
                     device_id, core_state.device_count);

            announce_devices();

            return ESP_OK;
        }
        prev = curr;
        curr = curr->next;
    }

    xSemaphoreGive(core_state.mutex);

    ESP_LOGW(TAG, "Device not found: %s", device_id);
    return SINRICPRO_ERR_DEVICE_NOT_FOUND;
}

/**
 * @brief Join the registered device ids with @p separator
 *
 * @return Number of bytes written, excluding the terminator
 */
static size_t build_device_ids(char *out, size_t out_len, char separator)
{
    size_t offset = 0;

    out[0] = '\0';

    xSemaphoreTake(core_state.mutex, portMAX_DELAY);
    for (sinricpro_device_t *device = core_state.devices;
         device != NULL && offset + 1 < out_len;
         device = device->next) {
        if (offset > 0) {
            out[offset++] = separator;
            out[offset] = '\0';
        }
        int written = snprintf(out + offset, out_len - offset, "%s", device->device_id);
        if (written < 0 || (size_t)written >= out_len - offset) {
            break;
        }
        offset += (size_t)written;
    }
    xSemaphoreGive(core_state.mutex);

    return offset;
}

/**
 * @brief Refresh the mDNS TXT record after the device list changed
 *
 * No-op before start(): the record is published there with the full list.
 */
static void announce_devices(void)
{
    if (!core_state.started) {
        return;
    }

    char device_ids[512];
    build_device_ids(device_ids, sizeof(device_ids), ',');
    sinricpro_mdns_update(device_ids);
}

static sinricpro_device_t* find_device(const char *device_id)
{
    sinricpro_device_t *device = core_state.devices;

    while (device != NULL) {
        if (strcmp(device->device_id, device_id) == 0) {
            return device;
        }
        device = device->next;
    }

    return NULL;
}

/* ========================================================================
 * Message Processing
 * ======================================================================== */

static void handle_request(cJSON *json_message, const sinricpro_msg_origin_t *origin)
{
    cJSON *payload = cJSON_GetObjectItem(json_message, "payload");
    if (payload == NULL) {
        ESP_LOGE(TAG, "No payload in request");
        return;
    }

    cJSON *device_id_item = cJSON_GetObjectItem(payload, "deviceId");
    cJSON *action_item = cJSON_GetObjectItem(payload, "action");
    cJSON *instance_id_item = cJSON_GetObjectItem(payload, "instanceId");
    cJSON *request_value = cJSON_GetObjectItem(payload, "value");

    if (device_id_item == NULL || action_item == NULL) {
        ESP_LOGE(TAG, "Missing deviceId or action in request");
        return;
    }

    const char *device_id = device_id_item->valuestring;
    const char *action = action_item->valuestring;
    const char *instance_id = instance_id_item ? instance_id_item->valuestring : NULL;

    ESP_LOGI(TAG, "Request: device=%s, action=%s", device_id, action);

    /* Find device */
    xSemaphoreTake(core_state.mutex, portMAX_DELAY);
    sinricpro_device_t *device = find_device(device_id);
    xSemaphoreGive(core_state.mutex);

    /* Prepare response */
    cJSON *response = cJSON_CreateObject();
    cJSON *response_header = cJSON_CreateObject();
    cJSON *response_payload = cJSON_CreateObject();
    cJSON *response_value = cJSON_CreateObject();

    cJSON_AddItemToObject(response, "header", response_header);
    cJSON_AddNumberToObject(response_header, "payloadVersion", 2);
    cJSON_AddNumberToObject(response_header, "signatureVersion", 1);

    cJSON_AddItemToObject(response, "payload", response_payload);
    cJSON_AddStringToObject(response_payload, "action", action);
    cJSON_AddNumberToObject(response_payload, "createdAt", core_state.timestamp);
    cJSON_AddStringToObject(response_payload, "deviceId", device_id);

    /* Copy replyToken and clientId from request */
    cJSON *reply_token = cJSON_GetObjectItem(payload, "replyToken");
    if (reply_token) {
        cJSON_AddStringToObject(response_payload, "replyToken", reply_token->valuestring);
    }

    cJSON *client_id = cJSON_GetObjectItem(payload, "clientId");
    if (client_id) {
        cJSON_AddStringToObject(response_payload, "clientId", client_id->valuestring);
    }

    if (instance_id) {
        cJSON_AddStringToObject(response_payload, "instanceId", instance_id);
    }

    cJSON_AddStringToObject(response_payload, "type", "response");
    cJSON_AddItemToObject(response_payload, "value", response_value);

    bool success = false;

    if (device != NULL && device->request_handler != NULL) {
        /* Call device request handler */
        success = device->request_handler(device_id, action, instance_id,
                                           request_value, response_value,
                                           device->user_data);
    } else {
        ESP_LOGW(TAG, "No handler for device: %s", device_id);
    }

    cJSON_AddBoolToObject(response_payload, "success", success);
    cJSON_AddStringToObject(response_payload, "message", success ? "OK" : "Device did not handle request");

    /* Queued with the origin it must go back on, so the send path can route it
     * without knowing which peer happens to be talking to us now. */
    char *response_str = cJSON_PrintUnformatted(response);
    if (response_str) {
        sinricpro_message_queue_push_with_origin(core_state.send_queue,
                                                  response_str, origin);
        free(response_str);
    }

    cJSON_Delete(response);
}

/**
 * @brief Answer a request whose signature did not verify
 *
 * Answering rather than dropping is deliberate: it lets a client tell a wrong
 * app secret apart from an unreachable device. It does mean the device answers
 * forged LAN packets.
 */
static void handle_invalid_signature(cJSON *json_message,
                                      const sinricpro_msg_origin_t *origin)
{
    cJSON *payload = cJSON_GetObjectItem(json_message, "payload");
    if (payload == NULL) {
        return;
    }

    cJSON *response = cJSON_CreateObject();
    if (response == NULL) {
        return;
    }

    cJSON *header = cJSON_CreateObject();
    cJSON *response_payload = cJSON_CreateObject();

    cJSON_AddItemToObject(response, "header", header);
    cJSON_AddNumberToObject(header, "payloadVersion", 2);
    cJSON_AddNumberToObject(header, "signatureVersion", 1);

    cJSON_AddItemToObject(response, "payload", response_payload);

    cJSON *action = cJSON_GetObjectItem(payload, "action");
    if (cJSON_IsString(action)) {
        cJSON_AddStringToObject(response_payload, "action", action->valuestring);
    }

    cJSON_AddNumberToObject(response_payload, "createdAt", core_state.timestamp);

    cJSON *device_id = cJSON_GetObjectItem(payload, "deviceId");
    if (cJSON_IsString(device_id)) {
        cJSON_AddStringToObject(response_payload, "deviceId", device_id->valuestring);
    }

    cJSON *reply_token = cJSON_GetObjectItem(payload, "replyToken");
    if (cJSON_IsString(reply_token)) {
        cJSON_AddStringToObject(response_payload, "replyToken", reply_token->valuestring);
    }

    cJSON *client_id = cJSON_GetObjectItem(payload, "clientId");
    if (cJSON_IsString(client_id)) {
        cJSON_AddStringToObject(response_payload, "clientId", client_id->valuestring);
    }

    cJSON *instance_id = cJSON_GetObjectItem(payload, "instanceId");
    if (cJSON_IsString(instance_id)) {
        cJSON_AddStringToObject(response_payload, "instanceId", instance_id->valuestring);
    }

    cJSON_AddStringToObject(response_payload, "type", "response");
    cJSON_AddItemToObject(response_payload, "value", cJSON_CreateObject());
    cJSON_AddBoolToObject(response_payload, "success", false);
    cJSON_AddStringToObject(response_payload, "message", "Signature is invalid");

    char *response_str = cJSON_PrintUnformatted(response);
    if (response_str) {
        sinricpro_message_queue_push_with_origin(core_state.send_queue,
                                                  response_str, origin);
        free(response_str);
    }

    cJSON_Delete(response);
}

static void handle_timestamp(cJSON *json_message)
{
    cJSON *timestamp_item = cJSON_GetObjectItem(json_message, "timestamp");
    if (timestamp_item && cJSON_IsNumber(timestamp_item)) {
        core_state.timestamp = (uint32_t)timestamp_item->valuedouble;
        ESP_LOGI(TAG, "Timestamp synchronized: %lu", core_state.timestamp);
    }
}

/**
 * @brief Verify and dispatch one received message, whatever transport it came on
 *
 * LAN requests land in the same capability callbacks as cloud requests; there
 * is no second dispatch path.
 */
static void process_incoming(const char *data, size_t length,
                             const sinricpro_msg_origin_t *origin)
{
    ESP_LOGD(TAG, "Received message (len=%zu): %.*s", length, (int)length, data);

    cJSON *json = cJSON_Parse(data);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return;
    }

    xSemaphoreTake(core_state.dispatch_mutex, portMAX_DELAY);

    /* Timestamp messages are unsigned by design. */
    if (cJSON_HasObjectItem(json, "timestamp")) {
        handle_timestamp(json);
        xSemaphoreGive(core_state.dispatch_mutex);
        cJSON_Delete(json);
        return;
    }

    cJSON *signature_obj = cJSON_GetObjectItem(json, "signature");
    cJSON *hmac_item = signature_obj ? cJSON_GetObjectItem(signature_obj, "HMAC") : NULL;

    /* Verified against the received bytes: the sender's key order and spacing
     * are its own, so a re-serialised object would not match. */
    const char *payload_start = NULL;
    size_t payload_len = 0;
    esp_err_t ret = SINRICPRO_ERR_SIGNATURE;

    if (cJSON_IsString(hmac_item) &&
        sinricpro_extract_payload_ref(data, &payload_start, &payload_len) == ESP_OK) {
        ret = sinricpro_verify_signature_n(core_state.config.app_secret,
                                            payload_start, payload_len,
                                            hmac_item->valuestring);
    }

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Signature verification failed");
        handle_invalid_signature(json, origin);
        xSemaphoreGive(core_state.dispatch_mutex);
        cJSON_Delete(json);
        return;
    }

    cJSON *payload = cJSON_GetObjectItem(json, "payload");
    if (payload) {
        cJSON *created_at = cJSON_GetObjectItem(payload, "createdAt");
        if (created_at && cJSON_IsNumber(created_at)) {
            core_state.timestamp = (uint32_t)created_at->valuedouble;
        }

        cJSON *type_item = cJSON_GetObjectItem(payload, "type");
        if (type_item && cJSON_IsString(type_item)) {
            const char *type = type_item->valuestring;

            if (strcmp(type, "request") == 0) {
                handle_request(json, origin);
            } else if (strcmp(type, "response") == 0) {
                ESP_LOGD(TAG, "Received response (ignored)");
            }
        }
    }

    xSemaphoreGive(core_state.dispatch_mutex);
    cJSON_Delete(json);
}

static void handle_received_message(const char *data, size_t length, void *context)
{
    const sinricpro_msg_origin_t origin = SINRICPRO_ORIGIN_WEBSOCKET;

    process_incoming(data, length, &origin);
}

#ifdef CONFIG_SINRICPRO_ENABLE_LOCAL_CONTROL
static void handle_udp_message(const char *data, size_t length,
                               const sinricpro_msg_origin_t *origin, void *context)
{
    process_incoming(data, length, origin);
}
#endif

/* ========================================================================
 * Event Sending
 * ======================================================================== */

esp_err_t sinricpro_core_send_event(const char *device_id,
                                     const char *action,
                                     const char *cause,
                                     cJSON *value)
{
    /* Ownership of `value` transfers here, so it must be released on the paths
     * that never attach it to a message. */
    if (!core_state.started) {
        cJSON_Delete(value);
        return SINRICPRO_ERR_NOT_STARTED;
    }

    /* Events are cloud-only; local control reports state to the app itself. */
    if (!sinricpro_ws_is_connected()) {
        cJSON_Delete(value);
        return SINRICPRO_ERR_NOT_CONNECTED;
    }

    /* Create event message */
    cJSON *event = cJSON_CreateObject();
    cJSON *header = cJSON_CreateObject();
    cJSON *payload = cJSON_CreateObject();
    cJSON *cause_obj = cJSON_CreateObject();

    cJSON_AddItemToObject(event, "header", header);
    cJSON_AddNumberToObject(header, "payloadVersion", 2);
    cJSON_AddNumberToObject(header, "signatureVersion", 1);

    cJSON_AddItemToObject(event, "payload", payload);
    cJSON_AddStringToObject(payload, "action", action);
    cJSON_AddItemToObject(payload, "cause", cause_obj);
    cJSON_AddStringToObject(cause_obj, "type", cause);
    cJSON_AddNumberToObject(payload, "createdAt", 0);  /* Will be set when sending */
    cJSON_AddStringToObject(payload, "deviceId", device_id);

    /* Generate unique replyToken (simple UUID-like string) */
    char reply_token[40];
    snprintf(reply_token, sizeof(reply_token), "%08lx-%04x-%04x",
             (unsigned long)esp_random(), (unsigned)(esp_random() & 0xFFFF),
             (unsigned)(esp_random() & 0xFFFF));
    cJSON_AddStringToObject(payload, "replyToken", reply_token);

    cJSON_AddStringToObject(payload, "type", "event");
    cJSON_AddItemToObject(payload, "value", value);

    /* Queue for sending */
    char *event_str = cJSON_PrintUnformatted(event);
    esp_err_t ret = ESP_FAIL;

    if (event_str) {
        ret = sinricpro_message_queue_push(core_state.send_queue, event_str);
        free(event_str);
    }

    cJSON_Delete(event);

    return ret;
}

/* ========================================================================
 * Send Task
 * ======================================================================== */

static void send_task_func(void *arg)
{
    ESP_LOGI(TAG, "Send task started");

    while (core_state.started) {
        /* Wait for message in queue */
        char *message = NULL;
        sinricpro_msg_origin_t origin = SINRICPRO_ORIGIN_WEBSOCKET;
        esp_err_t ret = sinricpro_message_queue_pop_with_origin(core_state.send_queue,
                                                                 &message, &origin,
                                                                 pdMS_TO_TICKS(1000));

        if (ret != ESP_OK || message == NULL) {
            continue;
        }

        /* Gate per message, never on cloud state as a whole: a device that has
         * never reached the cloud must still answer the LAN. */
        if (origin.transport == SINRICPRO_TRANSPORT_WEBSOCKET &&
            !sinricpro_ws_is_connected()) {
            ESP_LOGD(TAG, "Dropping websocket message - not connected");
            sinricpro_message_queue_free_message(message);
            continue;
        }

        cJSON *json = cJSON_Parse(message);
        sinricpro_message_queue_free_message(message);

        if (json == NULL) {
            ESP_LOGE(TAG, "Failed to parse queued message");
            continue;
        }

        cJSON *payload = cJSON_GetObjectItem(json, "payload");
        if (payload == NULL) {
            cJSON_Delete(json);
            continue;
        }

        /* Last mutation before signing. Nothing may touch the payload after
         * this point or the signature no longer covers what is transmitted. */
        cJSON_SetNumberValue(cJSON_GetObjectItem(payload, "createdAt"),
                              core_state.timestamp);

        char *signed_message = sinricpro_sign_message(core_state.config.app_secret, json);
        cJSON_Delete(json);

        if (signed_message == NULL) {
            ESP_LOGE(TAG, "Failed to sign message");
            continue;
        }

        ESP_LOGD(TAG, "Sending: %s", signed_message);

        switch (origin.transport) {
#ifdef CONFIG_SINRICPRO_ENABLE_LOCAL_CONTROL
        case SINRICPRO_TRANSPORT_UDP:
            /* LAN responses are never echoed to the cloud websocket. */
            sinricpro_udp_send(signed_message, origin.peer_addr, origin.peer_port);
            break;
#endif
        case SINRICPRO_TRANSPORT_WEBSOCKET:
        default:
            sinricpro_ws_send(signed_message, 0);
            break;
        }

        free(signed_message);
    }

    ESP_LOGI(TAG, "Send task stopped");
    vTaskDelete(NULL);
}

/* ========================================================================
 * Connection Handlers
 * ======================================================================== */

static void handle_connected(void *context)
{
    ESP_LOGI(TAG, "Connected to SinricPro server");
    esp_event_post(SINRICPRO_EVENT, SINRICPRO_EVENT_CONNECTED, NULL, 0, portMAX_DELAY);
}

static void handle_disconnected(void *context)
{
    ESP_LOGI(TAG, "Disconnected from SinricPro server");
    esp_event_post(SINRICPRO_EVENT, SINRICPRO_EVENT_DISCONNECTED, NULL, 0, portMAX_DELAY);
}

/* ========================================================================
 * Public API
 * ======================================================================== */

esp_err_t sinricpro_init(const sinricpro_config_t *config)
{
    if (config == NULL || config->app_key == NULL || config->app_secret == NULL) {
        ESP_LOGE(TAG, "Invalid configuration");
        return SINRICPRO_ERR_INVALID_CONFIG;
    }

    if (core_state.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return SINRICPRO_ERR_ALREADY_STARTED;
    }

    /* Create mutexes */
    core_state.mutex = xSemaphoreCreateMutex();
    core_state.dispatch_mutex = xSemaphoreCreateMutex();
    if (core_state.mutex == NULL || core_state.dispatch_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        if (core_state.mutex) {
            vSemaphoreDelete(core_state.mutex);
            core_state.mutex = NULL;
        }
        if (core_state.dispatch_mutex) {
            vSemaphoreDelete(core_state.dispatch_mutex);
            core_state.dispatch_mutex = NULL;
        }
        return ESP_ERR_NO_MEM;
    }

    /* Save configuration */
    memcpy(&core_state.config, config, sizeof(sinricpro_config_t));

    /* Set defaults */
    if (core_state.config.heartbeat_interval_ms == 0) {
        core_state.config.heartbeat_interval_ms = CONFIG_SINRICPRO_HEARTBEAT_INTERVAL_MS;
    }

    /* Create send queue */
    core_state.send_queue = sinricpro_message_queue_create(CONFIG_SINRICPRO_MESSAGE_QUEUE_SIZE);
    if (core_state.send_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create send queue");
        vSemaphoreDelete(core_state.mutex);
        core_state.mutex = NULL;
        vSemaphoreDelete(core_state.dispatch_mutex);
        core_state.dispatch_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    core_state.devices = NULL;
    core_state.device_count = 0;
    core_state.timestamp = 0;
    core_state.initialized = true;
    core_state.started = false;

    ESP_LOGI(TAG, "SinricPro initialized (version %s)", SINRICPRO_VERSION);

    return ESP_OK;
}

esp_err_t sinricpro_start(void)
{
    if (!core_state.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return SINRICPRO_ERR_NOT_INITIALIZED;
    }

    if (core_state.started) {
        ESP_LOGW(TAG, "Already started");
        return SINRICPRO_ERR_ALREADY_STARTED;
    }

    /* The websocket takes ';' separated ids; the mDNS TXT record takes CSV. */
    char device_ids[512];
    build_device_ids(device_ids, sizeof(device_ids), ';');

    if (device_ids[0] == '\0') {
        ESP_LOGW(TAG, "No devices registered");
    }

    ESP_LOGI(TAG, "Device IDs: %s", device_ids);

    /* Started before the cloud: local control must not depend on it. */
    core_state.started = true;

    BaseType_t task_ret = xTaskCreate(send_task_func, "sinricpro_send",
                                        4096, NULL, 5, &core_state.send_task);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create send task");
        core_state.started = false;
        return ESP_FAIL;
    }

#ifdef CONFIG_SINRICPRO_ENABLE_LOCAL_CONTROL
    if (sinricpro_udp_start(handle_udp_message, NULL) == ESP_OK) {
        char mdns_ids[512];
        build_device_ids(mdns_ids, sizeof(mdns_ids), ',');
        sinricpro_mdns_start(mdns_ids);
    }
#endif

    /* Initialize WebSocket */
    sinricpro_ws_callbacks_t ws_callbacks = {
        .on_receive = handle_received_message,
        .on_connected = handle_connected,
        .on_disconnected = handle_disconnected,
        .context = NULL
    };

    esp_err_t ret = sinricpro_ws_init(CONFIG_SINRICPRO_SERVER_URL,
                                        CONFIG_SINRICPRO_SERVER_PORT,
                                        core_state.config.app_key,
                                        device_ids,
                                        &ws_callbacks);
    /* An unreachable cloud is not a startup failure: the reconnect is armed and
     * local control is already serving. Callers check sinricpro_is_connected().
     * Only an invalid configuration is fatal. */
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WebSocket: %s, continuing without the cloud",
                 esp_err_to_name(ret));
    } else {
        ret = sinricpro_ws_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start WebSocket: %s, continuing without the cloud",
                     esp_err_to_name(ret));
        }
    }

    ESP_LOGI(TAG, "SinricPro started");

    return ESP_OK;
}

esp_err_t sinricpro_stop(void)
{
    if (!core_state.initialized) {
        return SINRICPRO_ERR_NOT_INITIALIZED;
    }

    if (!core_state.started) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping SinricPro...");

    /* Stop send task */
    core_state.started = false;
    if (core_state.send_task) {
        /* Task will delete itself */
        vTaskDelay(pdMS_TO_TICKS(100));
        core_state.send_task = NULL;
    }

#ifdef CONFIG_SINRICPRO_ENABLE_LOCAL_CONTROL
    sinricpro_mdns_stop();
    sinricpro_udp_stop();
#endif

    /* Stop WebSocket */
    sinricpro_ws_stop();
    sinricpro_ws_deinit();

    ESP_LOGI(TAG, "SinricPro stopped");

    return ESP_OK;
}

esp_err_t sinricpro_deinit(void)
{
    if (!core_state.initialized) {
        return ESP_OK;
    }

    /* Stop if running */
    sinricpro_stop();

    /* Clear send queue */
    if (core_state.send_queue) {
        sinricpro_message_queue_destroy(core_state.send_queue);
        core_state.send_queue = NULL;
    }

    /* Delete mutexes */
    if (core_state.mutex) {
        vSemaphoreDelete(core_state.mutex);
        core_state.mutex = NULL;
    }

    if (core_state.dispatch_mutex) {
        vSemaphoreDelete(core_state.dispatch_mutex);
        core_state.dispatch_mutex = NULL;
    }

    core_state.initialized = false;

    ESP_LOGI(TAG, "SinricPro deinitialized");

    return ESP_OK;
}

bool sinricpro_is_connected(void)
{
    if (!core_state.initialized || !core_state.started) {
        return false;
    }

    return sinricpro_ws_is_connected();
}

bool sinricpro_local_control_is_running(void)
{
#ifdef CONFIG_SINRICPRO_ENABLE_LOCAL_CONTROL
    return sinricpro_udp_is_running();
#else
    return false;
#endif
}

uint32_t sinricpro_get_timestamp(void)
{
    return core_state.timestamp;
}

const char* sinricpro_get_version(void)
{
    return SINRICPRO_VERSION;
}
