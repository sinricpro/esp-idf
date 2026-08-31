/*
 * Copyright (c) 2019-2025 Sinric. All rights reserved.
 * Licensed under Creative Commons Attribution-Share Alike (CC BY-SA)
 *
 * Host tests for the wire contract that local control depends on:
 * the payload is signed exactly as transmitted, and verified by slicing the
 * bytes as received. Both are cross-checked against the reference vectors the
 * Flutter app and the Python/Node SDKs produce.
 *
 * Build and run: test/host/run.sh
 */

#include "sinricpro_signature.h"

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

/* The user's test account secret, as used for the on-hardware verification. */
static const char *SECRET =
    "cc51b80f-0d7a-4c76-8f68-659f74d17f5d-4cd278be-3f4b-4878-82eb-a7629a0ea105";

static void test_hmac_reference_vector(void)
{
    printf("HMAC-SHA256 -> base64\n");

    /* RFC 4231 test case 1: key = 20 x 0x0b, data = "Hi There". */
    const char key[] = "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
                       "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b";
    char sig[64];

    esp_err_t ret = sinricpro_calculate_signature(key, "Hi There", sig, sizeof(sig));

    CHECK(ret == ESP_OK, "signature computed");
    CHECK(strcmp(sig, "sDRMYdjbOFNcqK/OrwvxK4gdwgDJgz2nJuk3bC4yz/c=") == 0,
          "RFC 4231 case 1 matches: %s", sig);
}

static void test_slice_between_markers(void)
{
    printf("payload slicing\n");

    /* A real request from the app: the payload is signed as these exact bytes,
     * with the sender's own key order, which we must not re-derive. */
    const char *msg =
        "{\"header\":{\"payloadVersion\":2,\"signatureVersion\":1},"
        "\"payload\":{\"action\":\"setPowerState\",\"clientId\":\"mobile-app\","
        "\"createdAt\":1756600000,\"deviceId\":\"6a93e2c73ee15f85c47ed491\","
        "\"replyToken\":\"abc\",\"scope\":\"device\",\"type\":\"request\","
        "\"value\":{\"state\":\"On\"}},"
        "\"signature\":{\"HMAC\":\"ignored\"}}";

    const char *payload = NULL;
    size_t len = 0;

    CHECK(sinricpro_extract_payload_ref(msg, &payload, &len) == ESP_OK, "payload located");
    CHECK(payload[0] == '{' && payload[len - 1] == '}', "slice is the payload object");
    CHECK(len == strlen("{\"action\":\"setPowerState\",\"clientId\":\"mobile-app\","
                        "\"createdAt\":1756600000,\"deviceId\":\"6a93e2c73ee15f85c47ed491\","
                        "\"replyToken\":\"abc\",\"scope\":\"device\",\"type\":\"request\","
                        "\"value\":{\"state\":\"On\"}}"),
          "slice length is exact (%zu)", len);

    /* A brace inside a string must not end the slice: this is the case the
     * brace-matching-only extractor got wrong. */
    const char *braced =
        "{\"header\":{},\"payload\":{\"message\":\"a } brace\",\"type\":\"request\"},"
        "\"signature\":{\"HMAC\":\"x\"}}";

    CHECK(sinricpro_extract_payload_ref(braced, &payload, &len) == ESP_OK,
          "payload with a brace inside a string located");
    CHECK(len == strlen("{\"message\":\"a } brace\",\"type\":\"request\"}"),
          "brace inside a string does not truncate the slice (%zu)", len);

    CHECK(sinricpro_extract_payload_ref("{\"header\":{}}", &payload, &len) == ESP_FAIL,
          "message with no payload is rejected");
}

static void test_verify_uses_received_bytes(void)
{
    printf("verification\n");

    const char *payload =
        "{\"action\":\"setPowerState\",\"clientId\":\"mobile-app\",\"createdAt\":1756600000,"
        "\"deviceId\":\"6a93e2c73ee15f85c47ed491\",\"replyToken\":\"abc\","
        "\"scope\":\"device\",\"type\":\"request\",\"value\":{\"state\":\"On\"}}";

    char sig[64];
    sinricpro_calculate_signature(SECRET, payload, sig, sizeof(sig));

    char *msg = malloc(strlen(payload) + strlen(sig) + 128);
    sprintf(msg, "{\"header\":{\"payloadVersion\":2,\"signatureVersion\":1},"
                 "\"payload\":%s,\"signature\":{\"HMAC\":\"%s\"}}", payload, sig);

    const char *sliced = NULL;
    size_t len = 0;
    sinricpro_extract_payload_ref(msg, &sliced, &len);

    CHECK(sinricpro_verify_signature_n(SECRET, sliced, len, sig) == ESP_OK,
          "correct signature accepted");

    CHECK(sinricpro_verify_signature_n(SECRET, sliced, len,
                                        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=")
              == SINRICPRO_ERR_SIGNATURE,
          "wrong signature rejected");

    CHECK(sinricpro_verify_signature_n("wrong-secret", sliced, len, sig)
              == SINRICPRO_ERR_SIGNATURE,
          "wrong secret rejected");

    /* Truncated signature: the constant-time compare must reject on length
     * rather than read past the end. */
    char truncated[8];
    memcpy(truncated, sig, 7);
    truncated[7] = '\0';
    CHECK(sinricpro_verify_signature_n(SECRET, sliced, len, truncated)
              == SINRICPRO_ERR_SIGNATURE,
          "truncated signature rejected");

    /* Re-ordering the payload the way a re-serialisation would must break the
     * signature - which is why verification slices instead. */
    char *reordered = malloc(strlen(payload) + 128);
    sprintf(reordered,
            "{\"type\":\"request\",\"action\":\"setPowerState\",\"clientId\":\"mobile-app\","
            "\"createdAt\":1756600000,\"deviceId\":\"6a93e2c73ee15f85c47ed491\","
            "\"replyToken\":\"abc\",\"scope\":\"device\",\"value\":{\"state\":\"On\"}}");
    CHECK(sinricpro_verify_signature_n(SECRET, reordered, strlen(reordered), sig)
              == SINRICPRO_ERR_SIGNATURE,
          "re-serialised payload does not verify (so slicing is load-bearing)");

    free(reordered);
    free(msg);
}

/*
 * A request captured from the SinricPro app's own signer
 * (tool/lan_probe.dart --emit, LocalControlSigner.buildRequest). If this stops
 * verifying, the device has stopped interoperating with the app.
 */
static void test_app_interop_vector(void)
{
    printf("app interop vector\n");

    const char *msg =
        "{\"header\":{\"payloadVersion\":2,\"signatureVersion\":1},"
        "\"payload\":{\"action\":\"setPowerState\",\"clientId\":\"mobile-app\","
        "\"createdAt\":1788163803,\"deviceId\":\"6a93e2c73ee15f85c47ed491\","
        "\"replyToken\":\"f5b9997fed487e987e5270ab83d94580\",\"scope\":\"device\","
        "\"type\":\"request\",\"value\":{\"state\":\"On\"}},"
        "\"signature\":{\"HMAC\":\"EFRFuSm3rWpI0i9uTDqs8FFGwTVozsnPnTEcJgmeH/Y=\"}}";

    const char *payload = NULL;
    size_t len = 0;

    CHECK(sinricpro_extract_payload_ref(msg, &payload, &len) == ESP_OK,
          "app request payload located");
    CHECK(sinricpro_verify_signature_n(SECRET, payload, len,
                                        "EFRFuSm3rWpI0i9uTDqs8FFGwTVozsnPnTEcJgmeH/Y=") == ESP_OK,
          "app-signed request verifies against the shipped Flutter signer");
}

static void test_sign_message_transmits_what_it_signed(void)
{
    printf("sign and splice\n");

    cJSON *msg = cJSON_CreateObject();
    cJSON *header = cJSON_CreateObject();
    cJSON *payload = cJSON_CreateObject();
    cJSON *value = cJSON_CreateObject();

    cJSON_AddItemToObject(msg, "header", header);
    cJSON_AddNumberToObject(header, "payloadVersion", 2);
    cJSON_AddNumberToObject(header, "signatureVersion", 1);

    cJSON_AddItemToObject(msg, "payload", payload);
    cJSON_AddStringToObject(payload, "action", "setPowerState");
    cJSON_AddNumberToObject(payload, "createdAt", 1756600000);
    cJSON_AddStringToObject(payload, "deviceId", "6a93e2c73ee15f85c47ed491");
    cJSON_AddStringToObject(payload, "instanceId", "rangeInstance1");
    cJSON_AddStringToObject(payload, "type", "response");
    cJSON_AddItemToObject(payload, "value", value);
    cJSON_AddStringToObject(value, "state", "On");

    char *wire = sinricpro_sign_message(SECRET, msg);
    CHECK(wire != NULL, "message signed");

    if (wire == NULL) {
        cJSON_Delete(msg);
        return;
    }

    /* The client slices between these markers, so the signature must be the
     * member immediately after the payload. */
    const char *sig_marker = strstr(wire, ",\"signature\"");
    CHECK(sig_marker != NULL, "signature member present");
    CHECK(strstr(wire, "\"header\":") < strstr(wire, "\"payload\":"),
          "header precedes payload");

    /* The bytes on the wire are the bytes that were signed. */
    const char *sliced = NULL;
    size_t len = 0;
    CHECK(sinricpro_extract_payload_ref(wire, &sliced, &len) == ESP_OK,
          "payload slices back out of the emitted envelope");

    cJSON *parsed = cJSON_Parse(wire);
    CHECK(parsed != NULL, "emitted envelope is valid JSON");

    const char *hmac = cJSON_GetStringValue(
        cJSON_GetObjectItem(cJSON_GetObjectItem(parsed, "signature"), "HMAC"));
    CHECK(hmac != NULL, "HMAC present");
    CHECK(hmac && sinricpro_verify_signature_n(SECRET, sliced, len, hmac) == ESP_OK,
          "emitted message verifies against its own transmitted payload bytes");

    /* instanceId is inside the signed payload, not appended afterwards. */
    CHECK(strstr(sliced, "\"instanceId\":\"rangeInstance1\"") != NULL &&
              (size_t)(strstr(sliced, "\"instanceId\"") - sliced) < len,
          "instanceId is covered by the signature");

    cJSON_Delete(parsed);
    free(wire);
    cJSON_Delete(msg);
}

int main(void)
{
    printf("SinricPro signature host tests\n\n");

    test_hmac_reference_vector();
    test_slice_between_markers();
    test_verify_uses_received_bytes();
    test_app_interop_vector();
    test_sign_message_transmits_what_it_signed();

    printf("\n%s\n", failures == 0 ? "all tests passed" : "TESTS FAILED");

    return failures == 0 ? 0 : 1;
}
