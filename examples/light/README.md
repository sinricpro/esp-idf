# SinricPro Smart Light Example

Control RGB LED with voice commands - brightness, color, and color temperature.

## Hardware

- ESP32 board
- RGB LED (common cathode)
- 3x 220Ω resistors

## Wiring

```
ESP32 GPIO25 → Red LED → 220Ω → GND
ESP32 GPIO26 → Green LED → 220Ω → GND
ESP32 GPIO27 → Blue LED → 220Ω → GND
```

## Features

- RGB color control
- Brightness 0-100%
- Color temperature 2200-7000K (warm to cool white)
- PWM-based LED control
- Smooth transitions

## Voice Commands

- "Alexa, turn on the light"
- "Alexa, set light brightness to 75"
- "Alexa, dim the light"
- "Alexa, set light to red"
- "Alexa, set light to warm white"
- "Alexa, make the light cooler"

## Build

```bash
cd examples/light
idf.py build flash monitor
```

## Configuration

Edit `main/light_example.c` - update WiFi and SinricPro credentials.

For different GPIO pins, modify:
```c
#define LEDC_RED_GPIO    (25)
#define LEDC_GREEN_GPIO  (26)
#define LEDC_BLUE_GPIO   (27)
```
