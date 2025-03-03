#pragma once

/**
 * @file timestamp.h
 * @author SinricPro (ESP-IDF Port)
 * @brief Timestamp utilities for SinricPro
 */

#include <string>
#include "sinricpro_namespace.h"

SINRICPRO_NAMESPACE_BEGIN

/**
 * @brief Initialize the timestamp module
 * 
 * Sets up SNTP client to synchronize time
 * @param time_zone Timezone to use (default: UTC)
 */
void initialize_timestamp(const char* time_zone = "UTC");

/**
 * @brief Get timestamp in ISO8601 format
 * 
 * @return std::string Timestamp string
 */
std::string get_iso_timestamp(void);

/**
 * @brief Get timestamp in milliseconds
 * 
 * @return uint64_t Timestamp in milliseconds
 */
uint64_t get_timestamp_ms(void);

/**
 * @brief Convert timestamp string to milliseconds
 * 
 * @param timestamp Timestamp string in ISO8601 format
 * @return uint64_t Timestamp in milliseconds
 */
uint64_t timestamp_to_ms(const std::string& timestamp);

SINRICPRO_NAMESPACE_END