/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * Host tests for websocket message reassembly. esp_websocket_client posts a
 * frame larger than its 2 KB buffer as several data events; before reassembly
 * each piece reached the JSON parser on its own, so any message above 2 KB
 * (a camera WebRTC offer is 3-6 KB) was silently lost.
 *
 * Build and run: test/host/run.sh
 */

#include "sinricpro_frame_assembler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...)                                        \
    do {                                                        \
        if (cond) {                                             \
            printf("  ok   " __VA_ARGS__);                      \
            printf("\n");                                       \
        } else {                                                \
            failures++;                                         \
            printf("  FAIL " __VA_ARGS__);                      \
            printf("   (%s:%d)\n", __FILE__, __LINE__);         \
        }                                                       \
    } while (0)

/* Feeds a message the way the websocket client posts it: chunk-sized events,
 * each with the total length and its own offset. */
static sinricpro_frame_result_t feed_in_chunks(sinricpro_frame_assembler_t *assembler,
                                               const char *text, size_t chunk,
                                               const char **message, size_t *message_len,
                                               int *pending_events)
{
    size_t total = strlen(text);
    sinricpro_frame_result_t result = SINRICPRO_FRAME_PENDING;
    *pending_events = 0;

    for (size_t offset = 0; offset < total; offset += chunk) {
        size_t len = total - offset < chunk ? total - offset : chunk;
        result = sinricpro_frame_assembler_feed(assembler, text + offset, len, offset, total,
                                                message, message_len);
        if (result == SINRICPRO_FRAME_PENDING) {
            (*pending_events)++;
        }
    }
    return result;
}

static char *make_message(size_t length)
{
    char *text = malloc(length + 1);
    for (size_t i = 0; i < length; i++) {
        text[i] = (char)('a' + i % 26);
    }
    text[length] = '\0';
    return text;
}

static void test_single_chunk(void)
{
    printf("single chunk\n");

    sinricpro_frame_assembler_t assembler;
    sinricpro_frame_assembler_init(&assembler, 16384);

    const char *message = NULL;
    size_t length = 0;
    const char *json = "{\"timestamp\":1757721600}";
    sinricpro_frame_result_t result = sinricpro_frame_assembler_feed(
        &assembler, json, strlen(json), 0, strlen(json), &message, &length);

    CHECK(result == SINRICPRO_FRAME_COMPLETE, "a message within one event completes immediately");
    CHECK(length == strlen(json) && strcmp(message, json) == 0, "bytes and length are preserved");

    result = sinricpro_frame_assembler_feed(&assembler, json, strlen(json), 0, 0, &message, &length);
    CHECK(result == SINRICPRO_FRAME_COMPLETE, "a zero total length means the chunk is the whole message");

    sinricpro_frame_assembler_free(&assembler);
}

static void test_offer_sized_message(void)
{
    printf("offer-sized message in 2 KB chunks\n");

    sinricpro_frame_assembler_t assembler;
    sinricpro_frame_assembler_init(&assembler, 16384);

    char *text = make_message(5500);
    const char *message = NULL;
    size_t length = 0;
    int pending = 0;
    sinricpro_frame_result_t result = feed_in_chunks(&assembler, text, 2048, &message, &length, &pending);

    CHECK(pending == 2, "the first two of three events are held back (%d)", pending);
    CHECK(result == SINRICPRO_FRAME_COMPLETE, "the last event completes the message");
    CHECK(length == 5500 && memcmp(message, text, 5500) == 0 && message[5500] == '\0',
          "reassembled bytes match and are terminated");

    free(text);
    sinricpro_frame_assembler_free(&assembler);
}

static void test_oversized_message(void)
{
    printf("message above the limit\n");

    sinricpro_frame_assembler_t assembler;
    sinricpro_frame_assembler_init(&assembler, 4096);

    char *big = make_message(9000);
    const char *message = NULL;
    size_t length = 0;
    int pending = 0;
    sinricpro_frame_result_t result = feed_in_chunks(&assembler, big, 2048, &message, &length, &pending);

    CHECK(result == SINRICPRO_FRAME_DROPPED, "dropped rather than parsed in pieces");
    CHECK(pending == 4, "reported once, on the last chunk, not per chunk (%d pending)", pending);

    const char *json = "{\"ok\":true}";
    result = sinricpro_frame_assembler_feed(&assembler, json, strlen(json), 0, strlen(json), &message, &length);
    CHECK(result == SINRICPRO_FRAME_COMPLETE && strcmp(message, json) == 0,
          "the next message is unaffected");

    free(big);
    sinricpro_frame_assembler_free(&assembler);
}

static void test_out_of_sequence(void)
{
    printf("chunks out of sequence\n");

    sinricpro_frame_assembler_t assembler;
    sinricpro_frame_assembler_init(&assembler, 16384);

    const char *message = NULL;
    size_t length = 0;
    char *text = make_message(3000);

    sinricpro_frame_result_t result = sinricpro_frame_assembler_feed(
        &assembler, text + 2048, 952, 2048, 3000, &message, &length);
    CHECK(result == SINRICPRO_FRAME_DROPPED, "a continuation with no message in progress is dropped");

    sinricpro_frame_assembler_feed(&assembler, text, 2048, 0, 3000, &message, &length);
    result = sinricpro_frame_assembler_feed(&assembler, text + 2048, 952, 2048, 3100, &message, &length);
    CHECK(result == SINRICPRO_FRAME_DROPPED, "a continuation claiming a different total is dropped");

    sinricpro_frame_assembler_feed(&assembler, text, 2048, 0, 3000, &message, &length);
    result = sinricpro_frame_assembler_feed(&assembler, text + 2048, 1000, 2048, 3000, &message, &length);
    CHECK(result == SINRICPRO_FRAME_DROPPED, "a chunk running past the total is dropped");

    sinricpro_frame_assembler_feed(&assembler, text, 2048, 0, 3000, &message, &length);
    result = sinricpro_frame_assembler_feed(&assembler, text, 2048, 0, 3000, &message, &length);
    CHECK(result == SINRICPRO_FRAME_PENDING, "a new message abandons an unfinished one");
    result = sinricpro_frame_assembler_feed(&assembler, text + 2048, 952, 2048, 3000, &message, &length);
    CHECK(result == SINRICPRO_FRAME_COMPLETE && memcmp(message, text, 3000) == 0,
          "and then completes normally");

    free(text);
    sinricpro_frame_assembler_free(&assembler);
}

int main(void)
{
    test_single_chunk();
    test_offer_sized_message();
    test_oversized_message();
    test_out_of_sequence();

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
