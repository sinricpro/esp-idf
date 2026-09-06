/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro_signature.h"
#include <stdlib.h>
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
 * @param[in]  payload      Data to sign
 * @param[in]  payload_len  Number of bytes to sign
 * @param[out] hmac_result  Output buffer, must hold 32 bytes
 *
 * @return ESP_OK on success, ESP_FAIL on failure
 */
static esp_err_t sinricpro_hmac_sha256(const char *secret,
                                       const char *payload,
                                       size_t payload_len,
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
                             (const uint8_t *)payload, payload_len,
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

    ret = mbedtls_md_hmac_update(&ctx, (const unsigned char *)payload, payload_len);
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
        ret = mbedtls_sha256_update(&ctx, (const unsigned char *)payload, payload_len);
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
 * @brief Constant-time comparison of two NUL-terminated strings
 *
 * Comparing signatures with strcmp() leaks how many leading bytes matched,
 * which is enough to narrow a forgery byte by byte.
 */
static bool sinricpro_const_time_equal(const char *a, const char *b)
{
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);

    if (len_a != len_b) {
        return false;
    }

    unsigned char diff = 0;
    for (size_t i = 0; i < len_a; i++) {
        diff |= (unsigned char)a[i] ^ (unsigned char)b[i];
    }

    return diff == 0;
}

esp_err_t sinricpro_calculate_signature_n(const char *secret,
                                          const char *payload,
                                          size_t payload_len,
                                          char *signature,
                                          size_t sig_len)
{
    if (secret == NULL || payload == NULL || signature == NULL || sig_len < 45) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    unsigned char hmac_result[32];  /* SHA256 produces 32 bytes */
    size_t olen = 0;

    esp_err_t err = sinricpro_hmac_sha256(secret, payload, payload_len, hmac_result);
    if (err != ESP_OK) {
        return err;
    }

    int ret = mbedtls_base64_encode((unsigned char *)signature, sig_len, &olen,
                                     hmac_result, sizeof(hmac_result));
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_base64_encode failed: %d", ret);
        return ESP_FAIL;
    }

    signature[olen] = '\0';

    return ESP_OK;
}

esp_err_t sinricpro_calculate_signature(const char *secret,
                                         const char *payload,
                                         char *signature,
                                         size_t sig_len)
{
    if (payload == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    return sinricpro_calculate_signature_n(secret, payload, strlen(payload),
                                            signature, sig_len);
}

esp_err_t sinricpro_verify_signature_n(const char *secret,
                                       const char *payload,
                                       size_t payload_len,
                                       const char *received_signature)
{
    if (secret == NULL || payload == NULL || received_signature == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    char calculated_signature[64];

    esp_err_t ret = sinricpro_calculate_signature_n(secret, payload, payload_len,
                                                     calculated_signature,
                                                     sizeof(calculated_signature));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to calculate signature");
        return ret;
    }

    if (sinricpro_const_time_equal(calculated_signature, received_signature)) {
        ESP_LOGD(TAG, "Signature verification passed");
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Signature verification failed");
    return SINRICPRO_ERR_SIGNATURE;
}

esp_err_t sinricpro_verify_signature(const char *secret,
                                      const char *payload,
                                      const char *received_signature)
{
    if (payload == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    return sinricpro_verify_signature_n(secret, payload, strlen(payload),
                                         received_signature);
}

esp_err_t sinricpro_extract_payload_ref(const char *json_message,
                                        const char **payload,
                                        size_t *payload_len)
{
    if (json_message == NULL || payload == NULL || payload_len == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    static const char kPayloadMarker[]   = "\"payload\":";
    static const char kSignatureMarker[] = ",\"signature\"";

    const char *start = strstr(json_message, kPayloadMarker);
    if (start == NULL) {
        ESP_LOGE(TAG, "\"payload\" field not found in JSON");
        return ESP_FAIL;
    }
    start += sizeof(kPayloadMarker) - 1;

    /* Wire contract: the signature always follows the payload, so the payload
     * is exactly the bytes between the two markers. */
    const char *end = strstr(start, kSignatureMarker);

    if (end == NULL) {
        /* No signature member, or it precedes the payload: fall back to
         * matching the payload object's own braces. */
        const char *p = strchr(start, '{');
        if (p == NULL) {
            ESP_LOGE(TAG, "Payload object not found");
            return ESP_FAIL;
        }
        start = p;

        int depth = 0;
        bool in_string = false;
        bool escaped = false;

        for (; *p != '\0'; p++) {
            if (in_string) {
                if (escaped) {
                    escaped = false;
                } else if (*p == '\\') {
                    escaped = true;
                } else if (*p == '"') {
                    in_string = false;
                }
                continue;
            }
            if (*p == '"') {
                in_string = true;
            } else if (*p == '{') {
                depth++;
            } else if (*p == '}') {
                if (--depth == 0) {
                    end = p + 1;
                    break;
                }
            }
        }

        if (end == NULL) {
            ESP_LOGE(TAG, "Payload object end not found");
            return ESP_FAIL;
        }
    }

    if (end <= start) {
        ESP_LOGE(TAG, "Empty payload");
        return ESP_FAIL;
    }

    *payload = start;
    *payload_len = (size_t)(end - start);

    return ESP_OK;
}

esp_err_t sinricpro_extract_payload(const char *json_message,
                                     char *payload,
                                     size_t payload_len)
{
    if (payload == NULL || payload_len == 0) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    const char *start = NULL;
    size_t len = 0;

    esp_err_t ret = sinricpro_extract_payload_ref(json_message, &start, &len);
    if (ret != ESP_OK) {
        return ret;
    }

    if (len >= payload_len) {
        ESP_LOGE(TAG, "Payload buffer too small (need %zu, have %zu)",
                 len + 1, payload_len);
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(payload, start, len);
    payload[len] = '\0';

    return ESP_OK;
}

/**
 * @brief Append bytes to a growable buffer
 *
 * On allocation failure the buffer is freed and *buf set to NULL, so a chain
 * of appends can be written without a check between every step.
 */
static bool sinricpro_str_append(char **buf, size_t *used, size_t *cap,
                                 const char *data, size_t len)
{
    if (*buf == NULL) {
        return false;
    }

    if (*used + len + 1 > *cap) {
        size_t new_cap = (*cap == 0) ? 256 : *cap;
        while (new_cap < *used + len + 1) {
            new_cap *= 2;
        }
        char *grown = realloc(*buf, new_cap);
        if (grown == NULL) {
            free(*buf);
            *buf = NULL;
            return false;
        }
        *buf = grown;
        *cap = new_cap;
    }

    memcpy(*buf + *used, data, len);
    *used += len;
    (*buf)[*used] = '\0';

    return true;
}

char *sinricpro_sign_message(const char *secret, cJSON *json)
{
    if (secret == NULL || json == NULL) {
        return NULL;
    }

    cJSON *payload = cJSON_GetObjectItem(json, "payload");
    if (payload == NULL) {
        ESP_LOGE(TAG, "Message has no payload to sign");
        return NULL;
    }

    /* Serialised once. Everything below splices this exact string, so nothing
     * can enter the payload between signing and transmission. */
    char *payload_str = cJSON_PrintUnformatted(payload);
    if (payload_str == NULL) {
        return NULL;
    }

    char signature[64];
    if (sinricpro_calculate_signature(secret, payload_str, signature,
                                       sizeof(signature)) != ESP_OK) {
        free(payload_str);
        return NULL;
    }

    size_t cap = 256;
    size_t used = 0;
    char *out = malloc(cap);
    if (out == NULL) {
        free(payload_str);
        return NULL;
    }
    out[0] = '\0';

    bool ok = sinricpro_str_append(&out, &used, &cap, "{", 1);

    /* Members other than payload/signature first, in their existing order.
     * Keys are SDK-generated identifiers, so they need no escaping. */
    for (cJSON *child = json->child; ok && child != NULL; child = child->next) {
        if (child->string == NULL ||
            strcmp(child->string, "payload") == 0 ||
            strcmp(child->string, "signature") == 0) {
            continue;
        }

        char *value = cJSON_PrintUnformatted(child);
        if (value == NULL) {
            ok = false;
            break;
        }

        ok = sinricpro_str_append(&out, &used, &cap, "\"", 1) &&
             sinricpro_str_append(&out, &used, &cap, child->string,
                                   strlen(child->string)) &&
             sinricpro_str_append(&out, &used, &cap, "\":", 2) &&
             sinricpro_str_append(&out, &used, &cap, value, strlen(value)) &&
             sinricpro_str_append(&out, &used, &cap, ",", 1);

        free(value);
    }

    /* Payload, then signature last: a receiver locates the payload by slicing
     * between "payload": and ,"signature". */
    ok = ok &&
         sinricpro_str_append(&out, &used, &cap, "\"payload\":", 10) &&
         sinricpro_str_append(&out, &used, &cap, payload_str, strlen(payload_str)) &&
         sinricpro_str_append(&out, &used, &cap, ",\"signature\":{\"HMAC\":\"", 22) &&
         sinricpro_str_append(&out, &used, &cap, signature, strlen(signature)) &&
         sinricpro_str_append(&out, &used, &cap, "\"}}", 3);

    free(payload_str);

    if (!ok) {
        free(out);
        return NULL;
    }

    return out;
}
