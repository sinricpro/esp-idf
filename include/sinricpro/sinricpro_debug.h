#pragma once

/**
 * @file sinricpro_debug.h
 * @author SinricPro (ESP-IDF Port)
 * @brief Debug macros for SinricPro ESP-IDF component
 */

#include "esp_log.h"
#include "sinricpro_config.h"

#if SINRICPRO_DEBUG

// Convert log level to ESP log level
#define SINRICPRO_LOG_LEVEL_NONE      ESP_LOG_NONE
#define SINRICPRO_LOG_LEVEL_ERROR     ESP_LOG_ERROR
#define SINRICPRO_LOG_LEVEL_WARN      ESP_LOG_WARN
#define SINRICPRO_LOG_LEVEL_INFO      ESP_LOG_INFO
#define SINRICPRO_LOG_LEVEL_DEBUG     ESP_LOG_DEBUG
#define SINRICPRO_LOG_LEVEL_VERBOSE   ESP_LOG_VERBOSE

// Default log level
#ifndef SINRICPRO_LOG_LEVEL
#define SINRICPRO_LOG_LEVEL           SINRICPRO_LOG_LEVEL_INFO
#endif

// Log macros
#define SINRICPRO_LOG_ERROR(tag, format, ...)   if (SINRICPRO_LOG_LEVEL >= SINRICPRO_LOG_LEVEL_ERROR) ESP_LOGE(tag, format, ##__VA_ARGS__)
#define SINRICPRO_LOG_WARN(tag, format, ...)    if (SINRICPRO_LOG_LEVEL >= SINRICPRO_LOG_LEVEL_WARN) ESP_LOGW(tag, format, ##__VA_ARGS__)
#define SINRICPRO_LOG_INFO(tag, format, ...)    if (SINRICPRO_LOG_LEVEL >= SINRICPRO_LOG_LEVEL_INFO) ESP_LOGI(tag, format, ##__VA_ARGS__)
#define SINRICPRO_LOG_DEBUG(tag, format, ...)   if (SINRICPRO_LOG_LEVEL >= SINRICPRO_LOG_LEVEL_DEBUG) ESP_LOGD(tag, format, ##__VA_ARGS__)
#define SINRICPRO_LOG_VERBOSE(tag, format, ...) if (SINRICPRO_LOG_LEVEL >= SINRICPRO_LOG_LEVEL_VERBOSE) ESP_LOGV(tag, format, ##__VA_ARGS__)

#else

// Disable all logs if debug is disabled
#define SINRICPRO_LOG_ERROR(tag, format, ...)   
#define SINRICPRO_LOG_WARN(tag, format, ...)    
#define SINRICPRO_LOG_INFO(tag, format, ...)    
#define SINRICPRO_LOG_DEBUG(tag, format, ...)   
#define SINRICPRO_LOG_VERBOSE(tag, format, ...) 

#endif

// Function entry/exit tracking (useful for debugging)
#if SINRICPRO_DEBUG && defined(CONFIG_SINRICPRO_DEBUG_FUNCTION_TRACKING)
#define SINRICPRO_FUNCTION_BEGIN(tag)      ESP_LOGD(tag, "--> %s", __FUNCTION__)
#define SINRICPRO_FUNCTION_END(tag)        ESP_LOGD(tag, "<-- %s", __FUNCTION__)
#else
#define SINRICPRO_FUNCTION_BEGIN(tag)
#define SINRICPRO_FUNCTION_END(tag)
#endif