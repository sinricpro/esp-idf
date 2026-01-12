# SinricPro Motion Sensor Example

Detects motion using a PIR sensor and reports to Alexa/Google Home.

## Hardware Required

- ESP32 development board
- PIR motion sensor (HC-SR501 or similar)
- LED (optional, for visual indication)
- Jumper wires

## Wiring

```
PIR Sensor    →  ESP32
VCC           →  5V or 3.3V
GND           →  GND
OUT           →  GPIO4

LED           →  GPIO2 (with 220Ω resistor)
```

## Configuration

Edit `main/motion_sensor_example.c`:

```c
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASS      "YOUR_WIFI_PASSWORD"
#define DEVICE_ID      "YOUR_DEVICE_ID"
#define APP_KEY        "YOUR_APP_KEY"
#define APP_SECRET     "YOUR_APP_SECRET"
```

## Build and Flash

```bash
cd examples/motion_sensor
idf.py build flash monitor
```

## Features

- Automatic motion detection
- Real-time event reporting to SinricPro
- Rate limiting (1 event per 60 seconds)
- LED indicator for motion state
- Alexa/Google Home notifications

## Voice Commands

- "Alexa, is there motion in the living room?"
- Configure Alexa routines based on motion detection

## Troubleshooting

**No motion detected:**
- Check PIR sensor wiring
- Adjust PIR sensitivity pot
- Wait 30-60 seconds for PIR to stabilize after power-on

**Rate limiting:**
- Motion sensor events are limited to 1 per 60 seconds
- This is a SinricPro server requirement
