/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "camera_snapshot.h"
#include <inttypes.h>
#include <stdio.h>
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "sinricpro.h"
#include "../core/sinricpro_device_internal.h"
#include "../core/sinricpro_signature.h"

static const char *TAG = "camera_snapshot";

#define SNAPSHOT_PATH       "/snapshot"
#define SNAPSHOT_PORT       443
/* Generous: the endpoint verifies the signature, stores the image and derives thumbnails before
 * replying, and a constrained device uploading over TLS is slow to begin with. */
#define SNAPSHOT_TIMEOUT_MS 30000
/* The receiving endpoint rejects anything larger, so an oversized frame is reported here rather
   than after uploading it. */
#define SNAPSHOT_MAX_BYTES  (512 * 1024)

esp_err_t sinricpro_camera_snapshot_upload(const char *device_id,
                                           const uint8_t *jpeg,
                                           size_t length)
{
    if (device_id == NULL || jpeg == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (length > SNAPSHOT_MAX_BYTES) {
        ESP_LOGE(TAG, "Snapshot is %u bytes; the server accepts at most %d. Lower the resolution "
                      "or the JPEG quality.",
                 (unsigned)length, SNAPSHOT_MAX_BYTES);
        return ESP_ERR_INVALID_SIZE;
    }

    /* The upload carries no websocket envelope, so it proves its origin with a signature over
     * deviceId + createdAt. The server also rejects a stale timestamp, which is why this uses the
     * one synchronised with the server rather than local time. */
    const char *secret = sinricpro_core_get_app_secret();
    uint32_t timestamp = sinricpro_get_timestamp();
    if (secret == NULL || timestamp == 0) {
        ESP_LOGE(TAG, "No credentials or server timestamp yet; connect before sending a snapshot");
        return ESP_ERR_INVALID_STATE;
    }

    char created_at[16];
    snprintf(created_at, sizeof(created_at), "%" PRIu32, timestamp);

    char payload[CONFIG_SINRICPRO_MAX_DEVICE_ID_LEN + sizeof(created_at)];
    snprintf(payload, sizeof(payload), "%s%s", device_id, created_at);

    char signature[64];
    esp_err_t err = sinricpro_calculate_signature(secret, payload, signature, sizeof(signature));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to sign the snapshot: %s", esp_err_to_name(err));
        return err;
    }

    esp_http_client_config_t config = {
        .host = CONFIG_SINRICPRO_CAMERA_URL,
        .path = SNAPSHOT_PATH,
        .port = SNAPSHOT_PORT,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = SNAPSHOT_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_header(client, "deviceId", device_id);
    esp_http_client_set_header(client, "createdAt", created_at);
    esp_http_client_set_header(client, "signature", signature);
    /* Deliberately no Content-Type: the endpoint runs express.raw() ahead of its own body reader,
     * and express.raw() only claims the stream when a Content-Type is present. Sending one leaves
     * the second reader waiting for data that has already been consumed, and the request never
     * gets a reply. The other SinricPro SDKs send none either. */
    /* Not copied: the image stays in the caller's frame buffer for the whole upload. */
    esp_http_client_set_post_field(client, (const char *)jpeg, (int)length);

    ESP_LOGI(TAG, "Uploading %u bytes to %s", (unsigned)length, CONFIG_SINRICPRO_CAMERA_URL);
    err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Snapshot upload failed: %s", esp_err_to_name(err));
        return err;
    }
    if (status != 200) {
        ESP_LOGE(TAG, "Snapshot rejected with HTTP %d", status);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Snapshot uploaded: %u bytes", (unsigned)length);
    return ESP_OK;
}
