/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef JPEG_STREAMER_H
#define JPEG_STREAMER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_camera.h"
#include "esp_peer.h"

/* Wire format per DataChannel message, shared with the SinricPro portal and app viewers:
 * four little-endian uint32 (magic, frame id, total length, offset) followed by up to
 * JPEG_STREAMER_CHUNK_SIZE bytes. Viewers reassemble by frame id and offset. */
#define JPEG_STREAMER_MAGIC       0x47504A53
#define JPEG_STREAMER_HEADER_SIZE 16
#define JPEG_STREAMER_CHUNK_SIZE  1024
#define JPEG_STREAMER_STALL_MS    1000

typedef enum {
    JPEG_STREAMER_IDLE,
    JPEG_STREAMER_SENDING,
    JPEG_STREAMER_COMPLETED,
    JPEG_STREAMER_ABANDONED,
} jpeg_streamer_result_t;

typedef struct {
    camera_fb_t *frame;
    size_t offset;
    uint32_t frame_id;
    uint32_t frame_interval_ms;
    size_t max_frame_bytes;
    uint32_t last_frame_ms;
    uint32_t frame_started_ms;
    uint32_t blocked_sends;

    /* Outcome of the most recent finished frame. Many blocked sends with little progress
     * means the DataChannel is wedged rather than slow, which quality reduction cannot fix. */
    uint32_t last_duration_ms;
    uint32_t last_blocked_sends;
    size_t last_sent_bytes;
    size_t last_frame_bytes;

    uint8_t packet[JPEG_STREAMER_HEADER_SIZE + JPEG_STREAMER_CHUNK_SIZE];
} jpeg_streamer_t;

void jpeg_streamer_init(jpeg_streamer_t *streamer, uint32_t frame_interval_ms, size_t max_frame_bytes);

/**
 * Sends at most one fragment per call, so ICE and SCTP keep running between fragments.
 * Call from the task that owns the peer.
 */
jpeg_streamer_result_t jpeg_streamer_loop(jpeg_streamer_t *streamer, esp_peer_handle_t peer, uint16_t stream_id);

/** Returns any frame in progress to the camera driver. */
void jpeg_streamer_reset(jpeg_streamer_t *streamer);

#endif /* JPEG_STREAMER_H */
