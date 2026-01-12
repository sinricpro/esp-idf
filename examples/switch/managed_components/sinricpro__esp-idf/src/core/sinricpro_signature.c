/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro_signature.h"
#include <string.h>
#include "mbedtls/md.h"
#include "mbedtls/base64.h"
#include "esp_log.h"

static const char *TAG = "sinricpro_signature";

/**
 * @brief Calculate HMAC-SHA256 signature and encode as base64
 *
 * @param[in]  secret      Secret key for HMAC
 * @param[in]  payload     Payload string to sign
 * @param[out] signature   Output buffer for base64-encoded signature
 * @param[in]  sig_len     Size of signature buffer (must be >= 45 bytes)
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_FAIL: HMAC or base64 encoding failed
 */
esp_err_t sinricpro_calculate_signature(const char *secret,
                                         const char *payload,
                                         char *signature,
                                         size_t sig_len)
{
    if (secret == NULL || payload == NULL || signature == NULL || sig_len < 45) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    unsigned char hmac_result[32];  /* SHA256 produces 32 bytes */
    size_t olen = 0;

    /* Calculate HMAC-SHA256 */
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

    mbedtls_md_init(&ctx);

    int ret = mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_md_setup failed: %d", ret);
        mbedtls_md_free(&ctx);
        return ESP_FAIL;
    }

    ret = mbedtls_md_hmac_starts(&ctx, (const unsigned char *)secret, strlen(secret));
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_md_hmac_starts failed: %d", ret);
        mbedtls_md_free(&ctx);
        return ESP_FAIL;
    }

    ret = mbedtls_md_hmac_update(&ctx, (const unsigned char *)payload, strlen(payload));
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_md_hmac_update failed: %d", ret);
        mbedtls_md_free(&ctx);
        return ESP_FAIL;
    }

    ret = mbedtls_md_hmac_finish(&ctx, hmac_result);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_md_hmac_finish failed: %d", ret);
        mbedtls_md_free(&ctx);
        return ESP_FAIL;
    }

    mbedtls_md_free(&ctx);

    /* Encode to base64 */
    ret = mbedtls_base64_encode((unsigned char *)signature, sig_len, &olen,
                                 hmac_result, sizeof(hmac_result));
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_base64_encode failed: %d", ret);
        return ESP_FAIL;
    }

    signature[olen] = '\0';  /* Null-terminate */

    ESP_LOGD(TAG, "Signature calculated: %s", signature);

    return ESP_OK;
}

/**
 * @brief Verify HMAC-SHA256 signature
 *
 * @param[in] secret             Secret key for HMAC
 * @param[in] payload            Payload string that was signed
 * @param[in] received_signature Base64-encoded signature to verify
 *
 * @return
 *     - ESP_OK: Signature is valid
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - SINRICPRO_ERR_SIGNATURE: Signature is invalid
 *     - ESP_FAIL: Calculation failed
 */
esp_err_t sinricpro_verify_signature(const char *secret,
                                      const char *payload,
                                      const char *received_signature)
{
    if (secret == NULL || payload == NULL || received_signature == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    char calculated_signature[64];

    esp_err_t ret = sinricpro_calculate_signature(secret, payload,
                                                    calculated_signature,
                                                    sizeof(calculated_signature));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to calculate signature");
        return ret;
    }

    /* Compare signatures */
    if (strcmp(calculated_signature, received_signature) == 0) {
        ESP_LOGD(TAG, "Signature verification passed");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Signature verification failed");
        ESP_LOGW(TAG, "Expected: %s", calculated_signature);
        ESP_LOGW(TAG, "Received: %s", received_signature);
        return SINRICPRO_ERR_SIGNATURE;
    }
}

/**
 * @brief Extract payload string from JSON message
 *
 * Extracts the "payload" field from a JSON message string for signature
 * calculation/verification.
 *
 * @param[in]  json_message  Complete JSON message string
 * @param[out] payload       Output buffer for extracted payload
 * @param[in]  payload_len   Size of payload buffer
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_FAIL: Failed to extract payload
 */
esp_err_t sinricpro_extract_payload(const char *json_message,
                                     char *payload,
                                     size_t payload_len)
{
    if (json_message == NULL || payload == NULL || payload_len == 0) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    /* Find "payload" field in JSON */
    const char *payload_start = strstr(json_message, "\"payload\":");
    if (payload_start == NULL) {
        ESP_LOGE(TAG, "\"payload\" field not found in JSON");
        return ESP_FAIL;
    }

    /* Skip to the start of the payload object */
    payload_start = strchr(payload_start, '{');
    if (payload_start == NULL) {
        ESP_LOGE(TAG, "Payload object not found");
        return ESP_FAIL;
    }

    /* Find the end of the payload object by matching braces */
    int brace_count = 0;
    const char *p = payload_start;
    const char *payload_end = NULL;

    while (*p != '\0') {
        if (*p == '{') {
            brace_count++;
        } else if (*p == '}') {
            brace_count--;
            if (brace_count == 0) {
                payload_end = p + 1;  /* Include closing brace */
                break;
            }
        }
        p++;
    }

    if (payload_end == NULL) {
        ESP_LOGE(TAG, "Payload object end not found");
        return ESP_FAIL;
    }

    size_t payload_size = payload_end - payload_start;

    if (payload_size >= payload_len) {
        ESP_LOGE(TAG, "Payload buffer too small (need %zu, have %zu)",
                 payload_size + 1, payload_len);
        return ESP_ERR_INVALID_SIZE;
    }

    /* Copy payload to output buffer */
    memcpy(payload, payload_start, payload_size);
    payload[payload_size] = '\0';

    ESP_LOGD(TAG, "Extracted payload: %s", payload);

    return ESP_OK;
}
