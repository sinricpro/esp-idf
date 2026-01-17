# Window AC Example

This example demonstrates how to control a window air conditioner using the SinricPro ESP-IDF component.

## Features

- **Power Control**: Turn the AC on/off
- **Mode Control**: AUTO, COOL, HEAT, ECO, OFF modes
- **Temperature Control**: Set target temperature
- **Fan Speed Control**: Adjust fan speed (0-100%)
- **Temperature Reporting**: Send current temperature and humidity to SinricPro

## Hardware Required

- ESP32 development board
- 3x Relays for controlling:
  - Heating element (GPIO 18)
  - Cooling element (GPIO 19)
  - Fan (GPIO 21)
- Temperature/humidity sensor (DHT22 recommended, simulated in this example)

## Configuration

1. Update WiFi credentials in `windowac_example.c`:
   ```c
   #define WIFI_SSID      "your_wifi_ssid"
   #define WIFI_PASS      "your_wifi_password"
   ```

2. Set up a Window AC device on [SinricPro](https://sinric.pro):
   - Create a new Window AC device
   - Copy the Device ID, App Key, and App Secret
   - Update in `windowac_example.c`:
     ```c
     #define DEVICE_ID      "your_device_id"
     #define APP_KEY        "your_app_key"
     #define APP_SECRET     "your_app_secret"
     ```

## GPIO Configuration

| Function | GPIO | Description |
|----------|------|-------------|
| RELAY_HEAT | 18 | Heating relay control |
| RELAY_COOL | 19 | Cooling relay control |
| RELAY_FAN | 21 | Fan relay control |

## Building and Flashing

```bash
cd examples/windowac
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Voice Commands

Once configured and connected, you can control the window AC with voice commands:

- "Alexa, turn on the AC"
- "Alexa, set the AC to cooling mode"
- "Alexa, set the AC temperature to 22 degrees"
- "Alexa, increase the fan speed"

## How It Works

1. The ESP32 connects to WiFi
2. Establishes a secure WebSocket connection to SinricPro
3. Waits for commands from Alexa or Google Home
4. Controls relays based on target temperature and mode
5. Periodically reports current temperature and humidity

## Temperature Control Logic

- **COOL Mode**: Activates cooling when current temp > target temp
- **HEAT Mode**: Activates heating when current temp < target temp
- **AUTO Mode**: Automatically switches between heating and cooling
- **ECO Mode**: Uses wider temperature range for efficiency
