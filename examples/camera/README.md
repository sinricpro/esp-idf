# Camera Example (WebRTC live view)

Streams an ESP32 or ESP32-S3 camera to the SinricPro portal and app, from anywhere. On ESP32-S3 the video is H.264 on a normal WebRTC video track; elsewhere, and for viewers that ask for it, it is JPEG over the encrypted DataChannel. Signaling runs through the SinricPro connection, and STUN/TURN servers arrive with each viewer's offer, so viewing works outside your LAN. Viewers can change resolution and frame rate and toggle flash, flip and mirror, and quality drops automatically on slow links. XIAO ESP32S3 Sense also streams its onboard microphone.

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
| Peer connection, H.264 and JPEG streaming, viewer controls | [components/webrtc_camera](components/webrtc_camera/), built on Espressif's `esp_peer` |
| Pin mappings | [main/camera_boards.c](main/camera_boards.c) |

The SinricPro component itself does not depend on `esp_peer` or `esp32-camera`; the `webrtc_camera` component can be copied into your own project.

Signaling is a single offer/answer exchange without trickle ICE, so the session gathers every local candidate before answering, and the answer callback blocks for up to about 5 seconds. One viewer is served at a time; a new offer replaces the current viewer.

## Video: H.264 or JPEG

The firmware supports both, and the viewer's offer decides which one a session uses.

| | H.264 track | JPEG over the DataChannel |
| --- | --- | --- |
| Targets | ESP32-S3 (`CONFIG_CAMERA_H264`, on by default) | every target |
| Resolution | 320x240 | up to SVGA |
| Frame rate | about 10 fps | a few fps, higher at small sizes |
| Playback | a normal video element | frames reassembled and drawn by the viewer |

`esp_h264` encodes in software on the S3, which is what caps the resolution; Espressif measures about 11 fps at 320x240. Encoding runs on its own task on the second core, so it does not disturb ICE, DTLS or the audio track.

The camera switches to YUV422 for an H.264 session and back to JPEG afterwards, so the sensor is re-initialised when a viewer connects and again when it leaves.

A viewer asks for a video track only when `getCameraCapabilities` reports `webrtcVideo`, which `sinricpro_camera_enable_webrtc_video()` sets. In the portal and the app, the **Smooth video** toggle switches between the two and reconnects. Older app and portal versions never offer a video track and keep getting JPEG.

The software encoder cannot produce a keyframe on demand, so a viewer's keyframe request (RTCP PLI) restarts the encoder, at most once every two seconds. A keyframe is sent every second anyway.

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

- Portal and app only. Alexa and Google Home streaming stays disabled for these cameras in the SinricPro cloud.
- WebRTC signaling needs the cloud connection: local control's UDP transport cannot carry an offer.
- Motion upload is not implemented yet. Snapshots are: the example answers `getSnapshot`, and during an H.264 session it compresses the YUV422 frame with `frame2jpg()` before uploading. The server accepts at most 512 KB per image.
