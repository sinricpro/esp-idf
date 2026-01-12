# SinricPro Switch Example

This example demonstrates how to use the SinricPro ESP-IDF component to create a smart switch that can be controlled via Alexa or Google Home.

## Features

- Voice control via Alexa/Google Home ("Alexa, turn on the switch")
- Physical button control (BOOT button toggles LED)
- Real-time state synchronization
- Auto-reconnection on network disruption
- Event rate limiting

## Hardware Required

- ESP32 development board
- Built-in LED (GPIO2)
- Built-in BOOT button (GPIO0)

## Configuration

1. **Get SinricPro Credentials:**
   - Sign up at [sinric.pro](https://sinric.pro)
   - Create a new Switch device
   - Note down your `APP_KEY`, `APP_SECRET`, and `DEVICE_ID`

2. **Modify the Example:**
   Edit `main/switch_example.c` and update these values:
   ```c
   #define WIFI_SSID      "YOUR_WIFI_SSID"
   #define WIFI_PASS      "YOUR_WIFI_PASSWORD"
   #define DEVICE_ID      "YOUR_DEVICE_ID"      /* 24-character hex */
   #define APP_KEY        "YOUR_APP_KEY"        /* UUID format */
   #define APP_SECRET     "YOUR_APP_SECRET"     /* Long secret key */
   ```

## How to Use

### Build and Flash

```bash
cd examples/switch
idf.py set-target esp32
idf.py menuconfig  # Optional: Configure SinricPro settings
idf.py build
idf.py flash monitor
```

### Test

1. **Via Voice:**
   - "Alexa, turn on the switch"
   - "Alexa, turn off the switch"
   - "Hey Google, turn on the switch"

2. **Via Physical Button:**
   - Press the BOOT button to toggle the LED
   - State is synchronized to SinricPro cloud

3. **Via Mobile App:**
   - Use the SinricPro mobile app to control the device

## Expected Output

```
I (XXX) switch_example: ==============================================
I (XXX) switch_example:  SinricPro ESP-IDF Switch Example
I (XXX) switch_example:  Version: 1.0.0
I (XXX) switch_example: ==============================================
I (XXX) switch_example: Connecting to WiFi...
I (XXX) switch_example: Got IP:192.168.1.100
I (XXX) switch_example: Connected to WiFi SSID:MyWiFi
I (XXX) switch_example: Initializing SinricPro...
I (XXX) switch_example: Creating switch device...
I (XXX) switch_example: Starting SinricPro...
I (XXX) switch_example: SinricPro started successfully!
I (XXX) switch_example: ✓ Connected to SinricPro server
I (XXX) switch_example: Button task started. Press BOOT button to toggle switch.
```

## Troubleshooting

### Cannot Connect to WiFi
- Check SSID and password
- Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Check signal strength

### Cannot Connect to SinricPro
- Verify APP_KEY and APP_SECRET are correct
- Check DEVICE_ID is exactly 24 hex characters
- Ensure internet connectivity
- Enable debug logging in menuconfig

### Alexa/Google Home Not Responding
- Ensure SinricPro skill is enabled
- Link your account in Alexa/Google Home app
- Discover devices in the app
- Check device name matches in SinricPro portal

## Code Structure

```c
/* Callbacks */
bool on_power_state(const char *device_id, bool *state, void *user_data);
bool on_setting(const char *device_id, const char *setting_id,
                const char *value, void *user_data);

/* Main Flow */
1. Initialize NVS and GPIO
2. Connect to WiFi
3. Initialize SinricPro
4. Create switch device
5. Register callbacks
6. Start SinricPro
7. Button task for physical control
```

## API Reference

See [component README](../../README.md) for full API documentation.

## License

Copyright (c) 2019-2025 Sinric. All rights reserved.

Licensed under Creative Commons Attribution-Share Alike (CC BY-SA 4.0)
