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
#include "cJSON.h"
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

/**
 * @brief Calculate a signature over an explicit byte range
 *
 * Same as sinricpro_calculate_signature() but the payload need not be
 * NUL-terminated, so a slice of a received buffer can be signed in place.
 *
 * @param[in]  secret       Secret key for HMAC
 * @param[in]  payload      Start of the payload bytes
 * @param[in]  payload_len  Number of payload bytes
 * @param[out] signature    Output buffer for base64-encoded signature
 * @param[in]  sig_len      Size of signature buffer (must be >= 45 bytes)
 *
 * @return ESP_OK, ESP_ERR_INVALID_ARG or ESP_FAIL
 */
esp_err_t sinricpro_calculate_signature_n(const char *secret,
                                          const char *payload,
                                          size_t payload_len,
                                          char *signature,
                                          size_t sig_len);

/**
 * @brief Verify a signature over an explicit byte range, in constant time
 *
 * @param[in] secret             Secret key for HMAC
 * @param[in] payload            Start of the payload bytes as received
 * @param[in] payload_len        Number of payload bytes
 * @param[in] received_signature Base64-encoded signature to verify
 *
 * @return ESP_OK, ESP_ERR_INVALID_ARG, SINRICPRO_ERR_SIGNATURE or ESP_FAIL
 */
esp_err_t sinricpro_verify_signature_n(const char *secret,
                                       const char *payload,
                                       size_t payload_len,
                                       const char *received_signature);

/**
 * @brief Locate the payload inside a received message without copying it
 *
 * Points into @p json_message; nothing is allocated. The slice is taken from
 * the bytes as received - re-serialising a parsed object would assume the
 * sender's key order and spacing, which are its own.
 *
 * @param[in]  json_message  Complete JSON message string as received
 * @param[out] payload       Receives a pointer into @p json_message
 * @param[out] payload_len   Receives the payload length in bytes
 *
 * @return ESP_OK or ESP_FAIL if the payload could not be located
 */
esp_err_t sinricpro_extract_payload_ref(const char *json_message,
                                        const char **payload,
                                        size_t *payload_len);

/**
 * @brief Sign a message and return the exact bytes to transmit
 *
 * The payload is serialised once and that string is spliced into the envelope,
 * so the bytes on the wire are the bytes that were signed. The signature is
 * emitted last, which is what lets a receiver find the payload by slicing
 * between "payload": and ,"signature".
 *
 * @param[in] secret  Secret key for HMAC
 * @param[in] json    Message object containing at least a "payload" member
 *
 * @return Serialised signed message (caller frees), or NULL on failure
 */
char *sinricpro_sign_message(const char *secret, cJSON *json);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_SIGNATURE_H */
