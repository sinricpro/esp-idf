/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_SIGNATURE_H
#define SINRICPRO_SIGNATURE_H

#include "esp_err.h"
#include "sinricpro_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

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
                                         size_t sig_len);

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
                                      const char *received_signature);

/**
 * @brief Extract payload string from JSON message
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
                                     size_t payload_len);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_SIGNATURE_H */
