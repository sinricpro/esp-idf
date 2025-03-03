#pragma once

/**
 * @file sinricpro_signature.h
 * @author SinricPro (ESP-IDF Port)
 * @brief Message signing utilities for SinricPro
 */

#include <string>
#include "mbedtls/md.h"
#include "sinricpro_namespace.h"

SINRICPRO_NAMESPACE_BEGIN

/**
 * @brief Create a signature for a message
 * 
 * @param app_key The application key
 * @param app_secret The application secret
 * @param message The message to sign
 * @return std::string The signature
 */
std::string create_signature(const std::string& app_key, const std::string& app_secret, const std::string& message);

/**
 * @brief Get the current timestamp in ISO8601 format
 * 
 * @return std::string Timestamp string
 */
std::string get_timestamp(void);

/**
 * @brief Sign a message with the SinricPro signature
 * 
 * @param app_key The application key
 * @param app_secret The application secret
 * @param message The message to sign
 * @return std::string The signed message
 */
std::string sign_message(const std::string& app_key, const std::string& app_secret, const std::string& message);

/**
 * @brief Verify a message signature
 * 
 * @param app_key The application key
 * @param app_secret The application secret
 * @param message The message to verify
 * @return true if signature is valid
 */
bool verify_message(const std::string& app_key, const std::string& app_secret, const std::string& message);

SINRICPRO_NAMESPACE_END