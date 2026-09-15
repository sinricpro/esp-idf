/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#ifndef SINRICPRO_FRAME_ASSEMBLER_H
#define SINRICPRO_FRAME_ASSEMBLER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Result of adding one received chunk
 */
typedef enum {
    SINRICPRO_FRAME_PENDING,   /**< More chunks of this message are expected */
    SINRICPRO_FRAME_COMPLETE,  /**< A whole message is available */
    SINRICPRO_FRAME_DROPPED,   /**< Too large, out of memory, or chunks out of sequence */
} sinricpro_frame_result_t;

/**
 * @brief Reassembles websocket messages delivered in pieces
 *
 * esp_websocket_client posts a frame larger than its buffer as several data
 * events, each carrying the frame's total length and its own offset. Parsing
 * each piece on its own hands the core truncated JSON.
 */
typedef struct {
    char *buffer;
    size_t capacity;
    size_t length;      /**< Bytes received for the message in progress */
    size_t expected;    /**< Total length of the message in progress; 0 when idle */
    size_t max_size;
    bool discarding;    /**< The message in progress is being dropped */
} sinricpro_frame_assembler_t;

void sinricpro_frame_assembler_init(sinricpro_frame_assembler_t *assembler, size_t max_size);

/**
 * @brief Abandon any partially received message, keeping the buffer
 */
void sinricpro_frame_assembler_reset(sinricpro_frame_assembler_t *assembler);

void sinricpro_frame_assembler_free(sinricpro_frame_assembler_t *assembler);

/**
 * @brief Add one received chunk
 *
 * @param[in]  assembler       Assembler
 * @param[in]  data            Chunk bytes
 * @param[in]  data_len        Chunk length
 * @param[in]  payload_offset  Offset of this chunk within the message
 * @param[in]  payload_len     Total message length; 0 means the chunk is the whole message
 * @param[out] message         On SINRICPRO_FRAME_COMPLETE, the NUL-terminated message,
 *                             owned by the assembler and valid until the next call
 * @param[out] message_len     On SINRICPRO_FRAME_COMPLETE, its length
 */
sinricpro_frame_result_t sinricpro_frame_assembler_feed(sinricpro_frame_assembler_t *assembler,
                                                        const char *data, size_t data_len,
                                                        size_t payload_offset, size_t payload_len,
                                                        const char **message, size_t *message_len);

#ifdef __cplusplus
}
#endif

#endif /* SINRICPRO_FRAME_ASSEMBLER_H */
