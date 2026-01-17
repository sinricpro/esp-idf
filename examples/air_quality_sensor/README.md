# Air Quality Sensor Example

This example demonstrates how to monitor air quality using a PM sensor and report data to SinricPro.

## Features

- **PM Monitoring**: Reports PM1.0, PM2.5, and PM10 levels
- **Air Quality Classification**: Categorizes air quality from "Good" to "Hazardous"
- **Real-time Dashboard**: View current air quality in the SinricPro app
- **Periodic Updates**: Sends readings every 60 seconds

## Hardware Required

- ESP32 development board
- Particulate Matter (PM) sensor (PMS5003, PMS7003, or similar)
  - Note: This example simulates sensor readings; integrate actual sensor for real use

## PM Sensor Integration (Optional)

For real air quality monitoring, connect a PM sensor like PMS5003:

| PM Sensor | ESP32 |
|-----------|-------|
| VCC | 5V |
| GND | GND |
| TX | RX (GPIO 16) |
| RX | TX (GPIO 17) |

## Configuration

1. Update WiFi credentials in `air_quality_sensor_example.c`:
   ```c
   #define WIFI_SSID      "your_wifi_ssid"
   #define WIFI_PASS      "your_wifi_password"
   ```

2. Set up an Air Quality Sensor device on [SinricPro](https://sinric.pro):
   - Create a new Air Quality Sensor device
   - Copy the Device ID, App Key, and App Secret
   - Update in `air_quality_sensor_example.c`:
     ```c
     #define DEVICE_ID      "your_device_id"
     #define APP_KEY        "your_app_key"
     #define APP_SECRET     "your_app_secret"
     ```

## Building and Flashing

```bash
cd examples/air_quality_sensor
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Air Quality Index (AQI) Levels

Based on PM2.5 readings:

| PM2.5 (ug/m3) | Air Quality Level |
|---------------|-------------------|
| 0-12 | Good |
| 13-35 | Moderate |
| 36-55 | Unhealthy for Sensitive Groups |
| 56-150 | Unhealthy |
| 151-250 | Very Unhealthy |
| 251+ | Hazardous |

## How It Works

1. The ESP32 connects to WiFi
2. Establishes a secure WebSocket connection to SinricPro
3. Reads PM values from the sensor (simulated in this example)
4. Categorizes air quality level
5. Sends readings to SinricPro dashboard every 60 seconds

## View Your Data

After the device is running:
1. Open the SinricPro app on your phone
2. Navigate to your Air Quality Sensor device
3. View real-time PM values and air quality level
4. Check historical data and trends

## Recommended PM Sensors

- **PMS5003**: Popular, accurate, affordable
- **PMS7003**: Similar to PMS5003 with UART interface
- **SDS011**: Laser-based PM sensor
- **Honeywell HPMA115S0**: Industrial-grade sensor

Note: Update the code to read from actual sensor hardware instead of using simulated values.
