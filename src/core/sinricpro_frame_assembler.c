/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * This file is part of the SinricPro ESP-IDF component
 * (https://github.com/sinricpro/esp-idf)
 */

#include "sinricpro_frame_assembler.h"
#include <stdlib.h>
#include <string.h>

void sinricpro_frame_assembler_init(sinricpro_frame_assembler_t *assembler, size_t max_size)
{
    memset(assembler, 0, sizeof(*assembler));
    assembler->max_size = max_size;
}

void sinricpro_frame_assembler_reset(sinricpro_frame_assembler_t *assembler)
{
    assembler->length = 0;
    assembler->expected = 0;
    assembler->discarding = false;
}

void sinricpro_frame_assembler_free(sinricpro_frame_assembler_t *assembler)
{
    free(assembler->buffer);
    assembler->buffer = NULL;
    assembler->capacity = 0;
    sinricpro_frame_assembler_reset(assembler);
}

sinricpro_frame_result_t sinricpro_frame_assembler_feed(sinricpro_frame_assembler_t *assembler,
                                                        const char *data, size_t data_len,
                                                        size_t payload_offset, size_t payload_len,
                                                        const char **message, size_t *message_len)
{
    if (payload_offset == 0) {
        /* A new message: whatever was in progress can no longer be completed. */
        sinricpro_frame_assembler_reset(assembler);
        if (payload_len == 0) {
            payload_len = data_len;
        }
        if (payload_len == 0) {
            return SINRICPRO_FRAME_PENDING;
        }

        assembler->expected = payload_len;
        if (payload_len > assembler->max_size) {
            assembler->discarding = true;
        } else if (payload_len + 1 > assembler->capacity) {
            char *grown = realloc(assembler->buffer, payload_len + 1);
            if (grown == NULL) {
                assembler->discarding = true;
            } else {
                assembler->buffer = grown;
                assembler->capacity = payload_len + 1;
            }
        }
    } else if (assembler->expected == 0 || payload_len != assembler->expected ||
               payload_offset != assembler->length) {
        sinricpro_frame_assembler_reset(assembler);
        return SINRICPRO_FRAME_DROPPED;
    }

    if (data_len > assembler->expected - assembler->length) {
        sinricpro_frame_assembler_reset(assembler);
        return SINRICPRO_FRAME_DROPPED;
    }

    if (!assembler->discarding) {
        memcpy(assembler->buffer + assembler->length, data, data_len);
    }
    assembler->length += data_len;

    if (assembler->length < assembler->expected) {
        return SINRICPRO_FRAME_PENDING;
    }

    /* A dropped message is reported once, on its last chunk, rather than per chunk. */
    bool discarded = assembler->discarding;
    size_t length = assembler->length;
    sinricpro_frame_assembler_reset(assembler);
    if (discarded) {
        return SINRICPRO_FRAME_DROPPED;
    }

    assembler->buffer[length] = '\0';
    *message = assembler->buffer;
    *message_len = length;
    return SINRICPRO_FRAME_COMPLETE;
}
