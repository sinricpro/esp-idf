/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "jpeg_streamer.h"
#include <string.h>
#include "webrtc_camera_priv.h"

static void put_le32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}

void jpeg_streamer_init(jpeg_streamer_t *streamer, uint32_t frame_interval_ms, size_t max_frame_bytes)
{
    memset(streamer, 0, sizeof(*streamer));
    streamer->frame_interval_ms = frame_interval_ms;
    streamer->max_frame_bytes = max_frame_bytes;
}

void jpeg_streamer_reset(jpeg_streamer_t *streamer)
{
    if (streamer->frame != NULL) {
        esp_camera_fb_return(streamer->frame);
        streamer->frame = NULL;
    }
    streamer->offset = 0;
}

static jpeg_streamer_result_t finish(jpeg_streamer_t *streamer, jpeg_streamer_result_t result)
{
    streamer->last_duration_ms = webrtc_camera_now_ms() - streamer->frame_started_ms;
    streamer->last_blocked_sends = streamer->blocked_sends;
    streamer->last_sent_bytes = streamer->offset;
    streamer->last_frame_bytes = streamer->frame != NULL ? streamer->frame->len : 0;
    streamer->blocked_sends = 0;
    /* reset() clears the offset and returns the frame, so the outcome is captured first. */
    jpeg_streamer_reset(streamer);
    return result;
}

jpeg_streamer_result_t jpeg_streamer_loop(jpeg_streamer_t *streamer, esp_peer_handle_t peer, uint16_t stream_id)
{
    if (streamer->frame == NULL) {
        uint32_t now = webrtc_camera_now_ms();
        if (now - streamer->last_frame_ms < streamer->frame_interval_ms) {
            return JPEG_STREAMER_IDLE;
        }
        streamer->last_frame_ms = now;

        streamer->frame = esp_camera_fb_get();
        if (streamer->frame == NULL) {
            return JPEG_STREAMER_IDLE;
        }
        streamer->frame_started_ms = now;
        streamer->offset = 0;

        /* Oversized frames count as congestion, so automatic quality compresses harder. */
        if (streamer->frame->format != PIXFORMAT_JPEG || streamer->frame->len > streamer->max_frame_bytes) {
            return finish(streamer, JPEG_STREAMER_ABANDONED);
        }
        streamer->frame_id++;
    }

    size_t remaining = streamer->frame->len - streamer->offset;
    size_t bytes = remaining < JPEG_STREAMER_CHUNK_SIZE ? remaining : JPEG_STREAMER_CHUNK_SIZE;

    put_le32(streamer->packet, JPEG_STREAMER_MAGIC);
    put_le32(streamer->packet + 4, streamer->frame_id);
    put_le32(streamer->packet + 8, (uint32_t)streamer->frame->len);
    put_le32(streamer->packet + 12, (uint32_t)streamer->offset);
    memcpy(streamer->packet + JPEG_STREAMER_HEADER_SIZE, streamer->frame->buf + streamer->offset, bytes);

    esp_peer_data_frame_t data = {
        .type = ESP_PEER_DATA_CHANNEL_DATA,
        .stream_id = stream_id,
        .data = streamer->packet,
        .size = (int)(bytes + JPEG_STREAMER_HEADER_SIZE),
    };
    int ret = esp_peer_send_data(peer, &data);
    if (ret == ESP_PEER_ERR_NONE) {
        streamer->offset += bytes;
    } else if (ret == ESP_PEER_ERR_WOULD_BLOCK) {
        streamer->blocked_sends++;
    }

    if (streamer->offset == streamer->frame->len) {
        return finish(streamer, JPEG_STREAMER_COMPLETED);
    }
    /* A stalled frame is abandoned so the viewer gets a fresh one instead of a late one. */
    if (webrtc_camera_now_ms() - streamer->frame_started_ms > JPEG_STREAMER_STALL_MS ||
        (ret != ESP_PEER_ERR_NONE && ret != ESP_PEER_ERR_WOULD_BLOCK)) {
        return finish(streamer, JPEG_STREAMER_ABANDONED);
    }
    return JPEG_STREAMER_SENDING;
}
