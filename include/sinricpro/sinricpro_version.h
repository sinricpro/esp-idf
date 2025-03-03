#pragma once

/**
 * @file sinricpro_version.h
 * @author SinricPro (ESP-IDF Port)
 * @brief Defines the SinricPro ESP-IDF component version
 */

#define SINRICPRO_VERSION_MAJOR       1
#define SINRICPRO_VERSION_MINOR       0
#define SINRICPRO_VERSION_PATCH       0
#define SINRICPRO_VERSION_REVISION    "esp-idf"

// Construct version string
#define SINRICPRO_VERSION_STR_HELPER(x)       #x
#define SINRICPRO_VERSION_STR(x)              SINRICPRO_VERSION_STR_HELPER(x)
#define SINRICPRO_VERSION       SINRICPRO_VERSION_STR(SINRICPRO_VERSION_MAJOR) "." \
                               SINRICPRO_VERSION_STR(SINRICPRO_VERSION_MINOR) "." \
                               SINRICPRO_VERSION_STR(SINRICPRO_VERSION_PATCH) "-" \
                               SINRICPRO_VERSION_REVISION