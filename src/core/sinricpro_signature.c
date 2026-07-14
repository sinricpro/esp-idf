/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro_signature.h"
#include <string.h>

/* mbedTLS 2.x exposes the version/config in version.h; 3.x+ in build_info.h */
#if defined(__has_include) && __has_include("mbedtls/build_info.h")
#include "mbedtls/build_info.h"
#else
#include "mbedtls/version.h"
#endif

/*
 * Pick an HMAC-SHA256 backend:
 *  - mbedTLS 4.x (removed the mbedtls_md_hmac_* API) or md-less PSA builds:
 *    PSA Crypto API
 *  - MBEDTLS_MD_C enabled (default on mbedTLS 2.x/3.x): classic md layer
 *  - otherwise (e.g. sdkconfig without CONFIG_MBEDTLS_MD_C): HMAC built
 *    directly on the always-available SHA-256 primitives
 */
#if MBEDTLS_VERSION_NUMBER >= 0x04000000 || \
    (!defined(MBEDTLS_MD_C) && defined(MBEDTLS_PSA_CRYPTO_C))
#define SINRICPRO_HMAC_BACKEND_PSA 1
#include "psa/crypto.h"
#elif defined(MBEDTLS_MD_C)
#define SINRICPRO_HMAC_BACKEND_MD 1
#include "mbedtls/md.h"
#else
#define SINRICPRO_HMAC_BACKEND_SHA256 1
#include "mbedtls/sha256.h"
#if MBEDTLS_VERSION_NUMBER < 0x03000000
#define mbedtls_sha256        mbedtls_sha256_ret
#define mbedtls_sha256_starts mbedtls_sha256_starts_ret
#define mbedtls_sha256_update mbedtls_sha256_update_ret
#define mbedtls_sha256_finish mbedtls_sha256_finish_ret
#endif
#endif

#include "mbedtls/base64.h"
#include "esp_log.h"

static const char *TAG = "sinricpro_signature";

/**
 * @brief Compute HMAC-SHA256 of payload using secret as the key
 *
 * @param[in]  secret       Secret key (NUL-terminated)
 * @param[in]  payload      Data to sign (NUL-terminated)
 * @param[out] hmac_result  Output buffer, must hold 32 bytes
 *
 * @return ESP_OK on success, ESP_FAIL on failure
 */
static esp_err_t sinricpro_hmac_sha256(const char *secret,
                                       const char *payload,
                                       unsigned char hmac_result[32])
{
#if defined(SINRICPRO_HMAC_BACKEND_PSA)
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_crypto_init failed: %d", (int)status);
        return ESP_FAIL;
    }

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);

    psa_key_id_t key_id = 0;
    status = psa_import_key(&attributes, (const uint8_t *)secret,
                            strlen(secret), &key_id);
    psa_reset_key_attributes(&attributes);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_import_key failed: %d", (int)status);
        return ESP_FAIL;
    }

    size_t mac_len = 0;
    status = psa_mac_compute(key_id, PSA_ALG_HMAC(PSA_ALG_SHA_256),
                             (const uint8_t *)payload, strlen(payload),
                             hmac_result, 32, &mac_len);
    psa_destroy_key(key_id);
    if (status != PSA_SUCCESS || mac_len != 32) {
        ESP_LOGE(TAG, "psa_mac_compute failed: %d", (int)status);
        return ESP_FAIL;
    }

    return ESP_OK;

#elif defined(SINRICPRO_HMAC_BACKEND_MD)
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
    return ESP_OK;

#else /* SINRICPRO_HMAC_BACKEND_SHA256 */
    /* HMAC per RFC 2104 over raw SHA-256 (block size 64 bytes) */
    unsigned char key_block[64] = {0};
    unsigned char pad[64];
    unsigned char inner_hash[32];
    size_t secret_len = strlen(secret);
    mbedtls_sha256_context ctx;
    int ret;

    if (secret_len > sizeof(key_block)) {
        ret = mbedtls_sha256((const unsigned char *)secret, secret_len, key_block, 0);
        if (ret != 0) {
            ESP_LOGE(TAG, "mbedtls_sha256 (key) failed: %d", ret);
            return ESP_FAIL;
        }
    } else {
        memcpy(key_block, secret, secret_len);
    }

    mbedtls_sha256_init(&ctx);

    for (size_t i = 0; i < sizeof(pad); i++) {
        pad[i] = key_block[i] ^ 0x36;
    }
    ret = mbedtls_sha256_starts(&ctx, 0);
    if (ret == 0) {
        ret = mbedtls_sha256_update(&ctx, pad, sizeof(pad));
    }
    if (ret == 0) {
        ret = mbedtls_sha256_update(&ctx, (const unsigned char *)payload, strlen(payload));
    }
    if (ret == 0) {
        ret = mbedtls_sha256_finish(&ctx, inner_hash);
    }

    if (ret == 0) {
        for (size_t i = 0; i < sizeof(pad); i++) {
            pad[i] = key_block[i] ^ 0x5C;
        }
        ret = mbedtls_sha256_starts(&ctx, 0);
    }
    if (ret == 0) {
        ret = mbedtls_sha256_update(&ctx, pad, sizeof(pad));
    }
    if (ret == 0) {
        ret = mbedtls_sha256_update(&ctx, inner_hash, sizeof(inner_hash));
    }
    if (ret == 0) {
        ret = mbedtls_sha256_finish(&ctx, hmac_result);
    }

    mbedtls_sha256_free(&ctx);

    if (ret != 0) {
        ESP_LOGE(TAG, "HMAC-SHA256 calculation failed: %d", ret);
        return ESP_FAIL;
    }

    return ESP_OK;
#endif
}

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
    esp_err_t err = sinricpro_hmac_sha256(secret, payload, hmac_result);
    if (err != ESP_OK) {
        return err;
    }

    /* Encode to base64 */
    int ret = mbedtls_base64_encode((unsigned char *)signature, sig_len, &olen,
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
