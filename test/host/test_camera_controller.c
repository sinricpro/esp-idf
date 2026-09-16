/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * Host tests for the camera WebRTC signaling contract shared with the portal,
 * the app and the Arduino SDK: the offer arrives base64-encoded, ICE servers
 * arrive as RTCIceServer entries whose "urls" is a string or an array, and the
 * answer goes back base64-encoded.
 *
 * Build and run: test/host/run.sh
 */

#define _POSIX_C_SOURCE 200809L

#include "camera_controller.h"
#include "mbedtls/base64.h"
#include "cJSON.h"

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

#define MAX_SEEN 16

/* What the offer callback received, and what it should return. */
static struct {
    int calls;
    char offer[256];
    size_t server_count;
    char url[MAX_SEEN][96];
    char username[MAX_SEEN][48];
    char credential[MAX_SEEN][48];
    const char *answer;
    bool result;
} seen;

static bool offer_callback(const char *device_id, const char *offer_sdp,
                           const sinricpro_ice_server_t *servers, size_t count,
                           char **answer_sdp, void *user_data)
{
    seen.calls++;
    snprintf(seen.offer, sizeof(seen.offer), "%s", offer_sdp);
    seen.server_count = count;
    for (size_t i = 0; i < count && i < MAX_SEEN; i++) {
        snprintf(seen.url[i], sizeof(seen.url[i]), "%s", servers[i].url);
        snprintf(seen.username[i], sizeof(seen.username[i]), "%s", servers[i].username);
        snprintf(seen.credential[i], sizeof(seen.credential[i]), "%s", servers[i].credential);
    }
    *answer_sdp = seen.answer != NULL ? strdup(seen.answer) : NULL;
    return seen.result;
}

static char *encode(const char *text)
{
    size_t length = 0;
    mbedtls_base64_encode(NULL, 0, &length, (const unsigned char *)text, strlen(text));
    char *out = malloc(length);
    mbedtls_base64_encode((unsigned char *)out, length, &length, (const unsigned char *)text, strlen(text));
    return out;
}

/* Runs one request through the controller; the caller deletes *response. */
static bool run(sinricpro_camera_controller_handle_t controller, const char *action,
                const char *request_json, cJSON **response)
{
    cJSON *request = request_json != NULL ? cJSON_Parse(request_json) : cJSON_CreateObject();
    *response = cJSON_CreateObject();
    bool handled = sinricpro_camera_controller_handle_request(controller, "5dc1564130xxxxxxxxxxxxxx",
                                                              action, request, *response);
    cJSON_Delete(request);
    return handled;
}

static void test_capabilities(void)
{
    printf("getCameraCapabilities\n");

    sinricpro_camera_controller_handle_t controller = sinricpro_camera_controller_create();
    cJSON *response = NULL;

    bool handled = run(controller, "getCameraCapabilities", NULL, &response);
    CHECK(handled, "answered even without a WebRTC callback");
    CHECK(cJSON_IsFalse(cJSON_GetObjectItem(response, "webrtc")) &&
          cJSON_IsFalse(cJSON_GetObjectItem(response, "webrtcAudio")),
          "firmware without a callback reports no WebRTC, so viewers ask for an update");
    cJSON_Delete(response);

    sinricpro_camera_controller_set_webrtc_audio(controller, true);
    run(controller, "getCameraCapabilities", NULL, &response);
    CHECK(cJSON_IsFalse(cJSON_GetObjectItem(response, "webrtcAudio")),
          "audio is not advertised without WebRTC");
    cJSON_Delete(response);

    sinricpro_camera_controller_set_webrtc_offer_callback(controller, offer_callback, NULL);
    run(controller, "getCameraCapabilities", NULL, &response);
    CHECK(cJSON_IsTrue(cJSON_GetObjectItem(response, "webrtc")) &&
          cJSON_IsTrue(cJSON_GetObjectItem(response, "webrtcAudio")),
          "a registered callback and audio are both reported");
    cJSON_Delete(response);

    sinricpro_camera_controller_destroy(controller);
}

static void test_offer_and_ice_servers(void)
{
    printf("getWebRTCAnswer\n");

    sinricpro_camera_controller_handle_t controller = sinricpro_camera_controller_create();
    sinricpro_camera_controller_set_webrtc_offer_callback(controller, offer_callback, NULL);

    const char *offer_sdp = "v=0\r\no=- 1 2 IN IP4 127.0.0.1\r\nm=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\n";
    const char *answer_sdp = "v=0\r\no=- 3 4 IN IP4 192.168.1.20\r\na=candidate:1 1 udp 2130706431 192.168.1.20 50000 typ host\r\n";
    char *offer_b64 = encode(offer_sdp);

    /* One TURN entry with three URLs, a bare STUN string, a junk entry, and an
     * array holding a non-string: every shape the flattening has to survive. */
    char request[1024];
    snprintf(request, sizeof(request),
             "{\"offer\":\"%s\",\"iceServers\":["
             "{\"urls\":[\"stun:turn.sinric.pro:3478\",\"turn:turn.sinric.pro:3478?transport=udp\","
             "\"turns:turn.sinric.pro:443?transport=tcp\"],"
             "\"username\":\"1757725200:user\",\"credential\":\"c2VjcmV0\"},"
             "{\"urls\":\"stun:stun.l.google.com:19302\"},"
             "42,"
             "{\"urls\":[7,\"turn:backup.example:3478\"]}]}",
             offer_b64);

    memset(&seen, 0, sizeof(seen));
    seen.answer = answer_sdp;
    seen.result = true;

    cJSON *response = NULL;
    bool handled = run(controller, "getWebRTCAnswer", request, &response);

    CHECK(handled, "a successful callback succeeds");
    CHECK(seen.calls == 1 && strcmp(seen.offer, offer_sdp) == 0, "the offer reaches the callback decoded");
    CHECK(seen.server_count == 5, "each URL becomes one entry, non-strings skipped (%zu)", seen.server_count);
    CHECK(strcmp(seen.url[1], "turn:turn.sinric.pro:3478?transport=udp") == 0 &&
          strcmp(seen.username[1], "1757725200:user") == 0 && strcmp(seen.credential[1], "c2VjcmV0") == 0,
          "URLs in one entry share its credentials");
    CHECK(strcmp(seen.url[3], "stun:stun.l.google.com:19302") == 0 &&
          seen.username[3][0] == '\0' && seen.credential[3][0] == '\0',
          "a string \"urls\" is accepted, with empty credentials when none are sent");
    CHECK(strcmp(seen.url[4], "turn:backup.example:3478") == 0, "the string after a non-string is kept");

    char *expected = encode(answer_sdp);
    cJSON *answer = cJSON_GetObjectItem(response, "answer");
    CHECK(cJSON_IsString(answer) && strcmp(answer->valuestring, expected) == 0,
          "the answer is returned base64-encoded");
    free(expected);
    cJSON_Delete(response);

    seen.result = false;
    handled = run(controller, "getWebRTCAnswer", request, &response);
    CHECK(!handled && cJSON_GetObjectItem(response, "answer") == NULL,
          "a failing callback fails and returns no answer");
    cJSON_Delete(response);

    seen.result = true;
    seen.answer = NULL;
    handled = run(controller, "getWebRTCAnswer", request, &response);
    CHECK(!handled, "success without an answer is reported as a failure");
    cJSON_Delete(response);

    free(offer_b64);
    sinricpro_camera_controller_destroy(controller);
}

static void test_rejected_requests(void)
{
    printf("rejected requests\n");

    sinricpro_camera_controller_handle_t controller = sinricpro_camera_controller_create();
    cJSON *response = NULL;

    CHECK(!run(controller, "getWebRTCAnswer", "{\"offer\":\"djA9\"}", &response),
          "an offer without a registered callback is not handled");
    cJSON_Delete(response);

    sinricpro_camera_controller_set_webrtc_offer_callback(controller, offer_callback, NULL);
    memset(&seen, 0, sizeof(seen));

    CHECK(!run(controller, "getWebRTCAnswer", "{\"offer\":\"@@@@\"}", &response) && seen.calls == 0,
          "an offer that is not base64 never reaches the callback");
    cJSON_Delete(response);

    CHECK(!run(controller, "getWebRTCAnswer", "{\"iceServers\":[]}", &response) && seen.calls == 0,
          "a request without an offer never reaches the callback");
    cJSON_Delete(response);

    CHECK(!run(controller, "setPowerState", "{\"state\":\"On\"}", &response),
          "other actions are left to other capabilities");
    cJSON_Delete(response);

    CHECK(sinricpro_camera_controller_owns_action("getWebRTCAnswer") &&
          sinricpro_camera_controller_owns_action("getCameraCapabilities") &&
          !sinricpro_camera_controller_owns_action("setPowerState") &&
          !sinricpro_camera_controller_owns_action(NULL),
          "action ownership");

    sinricpro_camera_controller_destroy(controller);
}

int main(void)
{
    test_capabilities();
    test_offer_and_ice_servers();
    test_rejected_requests();

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
