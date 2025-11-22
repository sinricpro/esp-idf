/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_EVENT_LIMITER_H
#define SINRICPRO_EVENT_LIMITER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Event limiter handle (opaque)
 */
typedef struct sinricpro_event_limiter* sinricpro_event_limiter_handle_t;

/**
 * @brief Rate limit intervals
 */
#define SINRICPRO_EVENT_LIMIT_STATE  1000    /**< State events: 1 per second */
#define SINRICPRO_EVENT_LIMIT_SENSOR 60000   /**< Sensor events: 1 per minute */

/**
 * @brief Create an event limiter
 *
 * @param[in] min_interval_ms Minimum interval between events (milliseconds)
 *
 * @return Limiter handle, or NULL on failure
 */
sinricpro_event_limiter_handle_t sinricpro_event_limiter_create(uint32_t min_interval_ms);

/**
 * @brief Check if an event can be sent
 *
 * Returns true if enough time has passed since the last event.
 * If true is returned, the limiter's internal timer is updated.
 *
 * @param[in] handle Limiter handle
 *
 * @return true if event can be sent, false if rate limited
 */
bool sinricpro_event_limiter_check(sinricpro_event_limiter_handle_t handle);

/**
 * @brief Get time until next event can be sent
 *
 * @param[in] handle Limiter handle
 *
 * @return Time in milliseconds until next event can be sent, or 0 if can send now
 */
uint32_t sinricpro_event_limiter_time_until_next(sinricpro_event_limiter_handle_t handle);

/**
 * @brief Reset the event limiter
 *
 * After reset, the next event will be allowed immediately.
 *
 * @param[in] handle Limiter handle
 */
void sinricpro_event_limiter_reset(sinricpro_event_limiter_handle_t handle);

/**
 * @brief Destroy event limiter and free resources
 *
 * @param[in] handle Limiter handle
 */
void sinricpro_event_limiter_destroy(sinricpro_event_limiter_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_EVENT_LIMITER_H */
