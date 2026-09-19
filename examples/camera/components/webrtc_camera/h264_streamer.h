/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef H264_STREAMER_H
#define H264_STREAMER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_peer.h"
#include "freertos/FreeRTOS.h"

/* H.264 video track for ESP32-S3, where esp_h264 encodes in software.
 *
 * The camera delivers YUV422 (YUYV), one of the two formats the software encoder accepts, so
 * frames go from the camera buffer into the encoder untouched. esp_peer packetises each encoded
 * frame into RTP.
 *
 * A QVGA frame costs roughly 90 ms to encode, which is why this runs on its own task: the session
 * task has to stay free for ICE, DTLS and the 20 ms audio pump. Encoded frames cross between the
 * two tasks through a free/ready queue pair, so neither waits for the other. */

typedef struct h264_streamer h264_streamer_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t fps;
    uint32_t bitrate;
    uint32_t task_stack_size;
    UBaseType_t task_priority;
} h264_streamer_config_t;

/** Creates the encoder and starts capturing. The camera must already be in YUV422 mode. */
esp_err_t h264_streamer_start(const h264_streamer_config_t *config, h264_streamer_t **out_streamer);

void h264_streamer_stop(h264_streamer_t *streamer);

/** Sends at most one encoded frame, so the caller keeps servicing the peer between frames. */
bool h264_streamer_send(h264_streamer_t *streamer, esp_peer_handle_t peer);

/** Answers an RTCP PLI. The software encoder cannot force an IDR, so the encoder is recreated,
 *  which emits a fresh IDR with SPS and PPS. Rate limited, since a viewer losing packets
 *  repeats the request. */
void h264_streamer_request_keyframe(h264_streamer_t *streamer);

void h264_streamer_set_fps(h264_streamer_t *streamer, uint8_t fps);

/** Frames encoded, and frames dropped because the viewer or the encoder fell behind. */
void h264_streamer_stats(const h264_streamer_t *streamer, uint32_t *encoded, uint32_t *dropped);

#endif /* H264_STREAMER_H */
