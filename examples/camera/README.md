# Camera Example (WebRTC live view)

Streams an ESP32, ESP32-S3 or ESP32-P4 camera to the SinricPro portal and app, from anywhere. On ESP32-S3 and ESP32-P4 the video is H.264 on a normal WebRTC video track (encoded in software on the S3, in hardware on the P4); elsewhere, and for viewers that ask for it, it is JPEG over the encrypted DataChannel. Signaling runs through the SinricPro connection, and STUN/TURN servers arrive with each viewer's offer, so viewing works outside your LAN. Viewers can change resolution and frame rate and toggle flash, flip and mirror, and quality drops automatically on slow links. XIAO ESP32S3 Sense also streams its onboard microphone.

## Requirements

- ESP-IDF 5.5 or later. `esp_peer` includes a prebuilt library that links only against 5.5 and newer.
- An ESP32 or ESP32-S3 camera board **with PSRAM**, or an ESP32-P4 board with on-board Wi-Fi (an ESP32-C6 running ESP-Hosted) and a MIPI-CSI camera; see [ESP32-P4](#esp32-p4)
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
idf.py set-target esp32        # or esp32s3, esp32p4
idf.py menuconfig
idf.py build flash monitor
```

| Board | Target |
| --- | --- |
| AI-Thinker ESP32-CAM (default on esp32) | esp32 |
| ESP-EYE, M5Camera A/B, ESP-WROVER-KIT, LILYGO T-Camera | esp32 |
| XIAO ESP32S3 Sense (default on esp32s3) | esp32s3 |
| Freenove ESP32-S3, ESP32-S3 WROOM (PWDN 38), GOOUUU ESP32-S3 | esp32s3 |
| ESP32-P4 board with a MIPI-CSI camera and an OV5647 sensor (default on esp32p4) | esp32p4 |

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
| Targets | ESP32-S3 and ESP32-P4 (`CONFIG_CAMERA_H264`, on by default) | every target |
| Resolution | S3: 320x240 or 640x480; P4: 800x640 or 1280x960 | up to SVGA |
| Frame rate | S3: about 3 fps at 320x240, 1 fps at 640x480; P4: 15 fps at 800x640, about 11 fps at 1280x960 | a few fps, higher at small sizes |
| Playback | a normal video element | frames reassembled and drawn by the viewer |
| Alexa and Google Home | S3: yes, at 640x480; P4: not yet verified | no |

`esp_h264` encodes in software on the S3, and that is the limit on both size and rate. Measured on a XIAO ESP32S3 Sense: about 300 ms per frame at 320x240 and a second or more at 640x480, because openh264's working set no longer fits internal RAM at the larger size and the encoder runs against PSRAM. Each size therefore declares the rate it can actually hold rather than an aspirational one, since rate control divides the bitrate by that figure: claiming 10 fps while delivering 1 made it compress roughly five times harder than the link required.

Encoding runs on its own task pinned to the second core, and the session task is pinned to the first. Sharing a core starves the idle task and trips the task watchdog, as well as slowing the encoder.

On the P4, `esp_peer_main_loop()` runs on a task of its own too. It waits up to the ICE receive timeout (500 ms, which slow TURN relays and distant smart displays need) for an incoming packet, so calling it from the task that sends video and DataChannel fragments held the P4's H.264 to 3 fps and its JPEG stream to a few kB/s. Other targets keep it on the session task, because a second task stack costs internal RAM that classic ESP32 does not have to spare. Either way its callbacks are queued to the session task, which owns all session state.

Viewers choose the resolution through the normal resolution control, which rebuilds the encoder mid-session; the next IDR carries the new SPS. A viewer with no DataChannel is a smart display, which cannot use that control and refuses anything below 480p, so those sessions start at 640x480.

The camera switches to the encoder's input format for an H.264 session (YUV422 on the S3, BGR888 on the P4) and back to JPEG afterwards, so the sensor is re-initialised when a viewer connects and again when it leaves.

A viewer asks for a video track only when `getCameraCapabilities` reports `webrtcVideo`, which `sinricpro_camera_enable_webrtc_video()` sets. In the portal and the app, the **Smooth video** toggle switches between the two and reconnects. Older app and portal versions never offer a video track and keep getting JPEG.

The software encoder cannot produce a keyframe on demand, so a viewer's keyframe request (RTCP PLI) restarts the encoder, at most once every two seconds. A keyframe is sent every second anyway.

## ESP32-P4

The P4 has no radio of its own and a MIPI-CSI camera interface instead of a parallel one, so three things differ from the other targets.

**Wi-Fi.** Use a board that carries an ESP32-C6 wired to the P4 over SDIO, such as the Espressif ESP32-P4-Function-EV-Board or a Waveshare ESP32-P4 Wi-Fi board. The C6 runs Espressif's ESP-Hosted co-processor firmware, which these boards ship with, and the `esp_hosted` and `esp_wifi_remote` components forward the example's ordinary `esp_wifi` calls to it. [sdkconfig.defaults.esp32p4](sdkconfig.defaults.esp32p4) uses the P4's standard SDIO pins (CLK 18, CMD 19, D0-D3 14-17, C6 reset on 54). A separate C6 board on jumper wires is not a good substitute: at the rate video needs, the link sees CRC errors, and ESP-Hosted resets the P4 on each one.

**Chip revision.** ESP-IDF builds for ESP32-P4 v3.0 and later by default, and a build for one revision range does not boot on the other. The boot log prints the revision. For a v1.x chip, uncomment `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` in `sdkconfig.defaults.esp32p4` and delete `sdkconfig` before building.

**Camera.** [components/p4_camera](components/p4_camera/) offers the `esp32-camera` API on top of Espressif's `esp_cam_sensor`, MIPI-CSI, ISP and JPEG drivers, so the session code is shared with the other targets. Only the sensor's I2C (SCCB) pins vary between boards; set them under **SinricPro Camera Example** (GPIO7 and GPIO8 by default). The supported sensor is the OV5647 (Raspberry Pi Camera Rev 1.3 and compatibles): the H.264 sizes are its native 800x640 and 1280x960 binned modes, and frames of any other size are refused with a log message. Use a camera ribbon cable; a Raspberry Pi display cable has a different pinout, and the sensor is never detected through one.

The image path is sensor RAW → ISP (denoise, demosaic, colour matrix with white balance, gamma, sharpen) → BGR888. The hardware JPEG encoder takes BGR888 directly. The hardware H.264 encoder on chips below v3.0 accepts only packed YUV420, and the ISP's own YUV420 output decodes green and colourless on those chips, so the Pixel Processing Accelerator converts each BGR888 frame first. That conversion is most of the per-frame cost: about 21 ms at 800x640 and 59 ms at 1280x960, with the encoder adding 8 ms and 24 ms. Both are bound by PSRAM bandwidth.

Measured on an ESP32-P4 rev v1.3 with an OV5647, in the SinricPro portal: 15 fps at 800x640 and about 11 fps at 1280x960. Smart displays have not been tested on the P4 yet.

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

- Amazon Alexa and Google Home need the H.264 track, so they work on ESP32-S3 (and should on ESP32-P4, which is not verified yet), and only with **H.264 video track** ticked in the portal's Camera Stream Configuration. Both were verified against an Echo and a Chromecast with Google TV at 640x480, which is the smallest size either accepts — around 1 fps, so the picture is slow. A board with a hardware encoder is the answer for a usable smart-display stream.
- Classic ESP32 streams JPEG over the DataChannel and stays portal and app only.
- WebRTC signaling needs the cloud connection: local control's UDP transport cannot carry an offer.
- Motion upload is not implemented yet. Snapshots are: the example answers `getSnapshot`, and during an H.264 session it compresses the raw frame with `frame2jpg()` before uploading. The server accepts at most 512 KB per image.
