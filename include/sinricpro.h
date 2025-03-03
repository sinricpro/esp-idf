#pragma once

/**
 * @file sinricpro.h
 * @author SinricPro (ESP-IDF Port)
 * @brief SinricPro ESP-IDF Component
 * 
 * This is the main include file for the SinricPro ESP-IDF component.
 * It provides connectivity to the SinricPro IoT cloud service for controlling
 * ESP32 devices through cloud services like Amazon Alexa.
 */

#include "sinricpro/sinricpro.h"
#include "sinricpro/sinricpro_switch.h"
#include "sinricpro/sinricpro_config.h"
#include "sinricpro/sinricpro_debug.h"
#include "sinricpro/sinricpro_device.h"
#include "sinricpro/sinricpro_device_interface.h"
#include "sinricpro/sinricpro_interface.h"
#include "sinricpro/sinricpro_namespace.h"
#include "sinricpro/sinricpro_queue.h"
#include "sinricpro/sinricpro_request.h"
#include "sinricpro/sinricpro_signature.h"
#include "sinricpro/sinricpro_version.h"
#include "sinricpro/sinricpro_websocket.h"
#include "sinricpro/timestamp.h"
#include "sinricpro/event_limiter.h"