# Camera Example (WebRTC live view)

Streams an ESP32 or ESP32-S3 camera to the SinricPro portal and app, from anywhere. Video is JPEG over an encrypted WebRTC DataChannel; signaling runs through the SinricPro connection, and STUN/TURN servers arrive with each viewer's offer, so viewing works outside your LAN. Viewers can change resolution and frame rate and toggle flash, flip and mirror, and quality drops automatically on slow links. XIAO ESP32S3 Sense also streams its onboard microphone.

## Requirements

- ESP-IDF 5.5 or later. `esp_peer` includes a prebuilt library that links only against 5.5 and newer.
- An ESP32 or ESP32-S3 camera board **with PSRAM**
- 4 MB of flash or more
- A Wi-Fi signal of **−75 dBm or better** at the board. Below about −80 dBm the Wi-Fi driver's transmit buffers stop recycling fast enough and the DTLS handshake cannot complete, even though free heap looks healthy.

## Portal setup

1. Create a device of type **Camera**.
2. In **Camera Stream Configuration**, set Board to **ESP32** and Streaming Protocol to **WebRTC**.
3. Copy the device ID, app key and app secret.

## Configure and build

Edit the credentials at the top of [main/camera_example.c](main/camera_example.c):

```c
#define WIFI_SSID      "WIFI_SSID"
#define WIFI_PASS      "WIFI_PASS"
#define DEVICE_ID      "DEVICE_ID"
#define APP_KEY        "APP_KEY"
#define APP_SECRET     "APP_SECRET"
```

Select the target, then your board under **SinricPro Camera Example → Camera board**:

```bash
cd examples/camera
idf.py set-target esp32        # or esp32s3
idf.py menuconfig
idf.py build flash monitor
```

| Board | Target |
| --- | --- |
| AI-Thinker ESP32-CAM (default on esp32) | esp32 |
| ESP-EYE, M5Camera A/B, ESP-WROVER-KIT, LILYGO T-Camera | esp32 |
| XIAO ESP32S3 Sense (default on esp32s3) | esp32s3 |
| Freenove ESP32-S3, ESP32-S3 WROOM (PWDN 38), GOOUUU ESP32-S3 | esp32s3 |

`sdkconfig.defaults.esp32s3` assumes octal PSRAM, as on the XIAO Sense and Freenove N8R8. For a board with quad PSRAM, set `CONFIG_SPIRAM_MODE_QUAD` instead.

Open **Preview** on the camera in the portal, or tap the camera in the app.

## How it works

| Piece | Where |
| --- | --- |
| `getCameraCapabilities`, `getWebRTCAnswer` | SinricPro component: `sinricpro_camera_on_webrtc_offer()` |
| Peer connection, JPEG streaming, viewer controls | [components/webrtc_camera](components/webrtc_camera/), built on Espressif's `esp_peer` |
| Pin mappings | [main/camera_boards.c](main/camera_boards.c) |

The SinricPro component itself does not depend on `esp_peer` or `esp32-camera`; the `webrtc_camera` component can be copied into your own project.

Signaling is a single offer/answer exchange without trickle ICE, so the session gathers every local candidate before answering, and the answer callback blocks for up to about 5 seconds. One viewer is served at a time; a new offer replaces the current viewer.

## Memory settings

[sdkconfig.defaults](sdkconfig.defaults) moves Wi-Fi, lwIP and mbedTLS allocations into PSRAM. On classic ESP32 that is what leaves enough contiguous internal RAM for the Wi-Fi driver's transmit buffers once the TLS connection to SinricPro is open. The example also keeps the DataChannel caches small on classic ESP32 for the same reason.

`CONFIG_FREERTOS_HZ=1000` matters too: the session sends one DataChannel fragment per tick, so at the default 100 Hz throughput is capped at about 100 kB/s.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Viewer shows one frozen frame, device logs `streaming: yes` | Wi-Fi signal; the log prints RSSI at connect and every 30 s |
| `dtlsState=connecting` in `chrome://webrtc-internals` | Wi-Fi signal and free internal heap in the 30-second log line |
| Viewer reports "firmware does not support live view" | The WebRTC offer callback is not registered |
| `Camera init failed` | Board selection in menuconfig, ribbon cable, and PSRAM mode |
| `PSRAM is not available` | `CONFIG_SPIRAM`, and octal versus quad PSRAM on ESP32-S3 |

## Limits

- Portal and app only. Alexa and Google Home need a native H.264 video track.
- WebRTC signaling needs the cloud connection: local control's UDP transport cannot carry an offer.
- Snapshot and motion upload are not implemented yet.
