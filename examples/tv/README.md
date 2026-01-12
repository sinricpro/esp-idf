# SinricPro TV Example

This example demonstrates how to control a TV device using SinricPro with voice commands via Alexa or Google Home.

## Features

This example showcases all TV capabilities:

- **Power Control** - Turn TV on/off
- **Volume Control** - Set or adjust volume (0-100)
- **Mute Control** - Mute/unmute audio
- **Media Control** - Play, pause, stop, next, previous
- **Input Selection** - Switch between HDMI1, HDMI2, AV, etc.
- **Channel Control** - Change channels by number or skip channels
- **Settings** - Configure TV settings

## Hardware Required

- ESP32, ESP32-S2, ESP32-S3, ESP32-C3, or ESP32-C6 development board
- USB cable for power supply and programming
- (Optional) LED connected to GPIO2 to visualize TV state
- Built-in BOOT button (GPIO0) for manual control

## How to Use

### 1. Setup SinricPro Account

1. Create a free account at [sinric.pro](https://sinric.pro)
2. Go to "Devices" and click "Add Device"
3. Select device type: **TV**
4. Give it a name (e.g., "Living Room TV")
5. Note down the **Device ID**, **App Key**, and **App Secret**

### 2. Configure the Example

Edit `main/tv_example.c` and update the following:

```c
/* WiFi Configuration */
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASS      "YOUR_WIFI_PASSWORD"

/* SinricPro Configuration */
#define DEVICE_ID      "YOUR_DEVICE_ID"      /* From SinricPro portal */
#define APP_KEY        "YOUR_APP_KEY"        /* From SinricPro portal */
#define APP_SECRET     "YOUR_APP_SECRET"     /* From SinricPro portal */
```

### 3. Build and Flash

```bash
cd examples/tv
idf.py set-target esp32  # or esp32s2, esp32s3, esp32c3, esp32c6
idf.py build
idf.py flash monitor
```

### 4. Voice Commands

Once the device is online, you can control it with voice commands:

**Power Control:**
- "Alexa, turn on the TV"
- "Alexa, turn off the TV"

**Volume Control:**
- "Alexa, set TV volume to 50"
- "Alexa, increase TV volume"
- "Alexa, decrease TV volume by 10"

**Mute Control:**
- "Alexa, mute the TV"
- "Alexa, unmute the TV"

**Input Selection:**
- "Alexa, change TV to HDMI 1"
- "Alexa, switch TV input to HDMI 2"

**Channel Control:**
- "Alexa, change TV to channel 5"
- "Alexa, next channel"
- "Alexa, previous channel"
- "Alexa, channel up"

**Media Control:**
- "Alexa, play" (when watching streaming apps)
- "Alexa, pause"
- "Alexa, stop"

## Physical Button Control

Press the built-in BOOT button (GPIO0) to toggle the TV power state. The built-in LED (GPIO2) will indicate the current state.

## Customization

### Modify GPIO Pins

Change the LED and button GPIO pins in `main/tv_example.c`:

```c
#define LED_GPIO       GPIO_NUM_2   /* Your LED GPIO */
#define BUTTON_GPIO    GPIO_NUM_0   /* Your button GPIO */
```

### Add Custom Actions

Implement custom logic in the callback functions:

```c
static bool on_media_control(const char *device_id, const char *control, void *user_data)
{
    if (strcmp(control, "Play") == 0) {
        /* Your play logic */
    } else if (strcmp(control, "Pause") == 0) {
        /* Your pause logic */
    }
    return true;
}
```

### Send TV Events

Report TV state changes to SinricPro:

```c
/* Power state changed */
sinricpro_tv_send_power_state_event(my_tv, true, SINRICPRO_CAUSE_PHYSICAL_INTERACTION);

/* Volume changed */
sinricpro_tv_send_volume_event(my_tv, 75, SINRICPRO_CAUSE_PHYSICAL_INTERACTION);

/* Channel changed */
sinricpro_tv_channel_t channel = {5, "HBO"};
sinricpro_tv_send_channel_event(my_tv, &channel, SINRICPRO_CAUSE_PHYSICAL_INTERACTION);
```

## Troubleshooting

**Device doesn't connect:**
- Check WiFi credentials
- Verify internet connection
- Ensure SinricPro credentials are correct

**Voice commands don't work:**
- Make sure device shows as "Online" in SinricPro portal
- Check that Alexa/Google Home skill is linked
- Try device discovery: "Alexa, discover devices"

**Rate limiting errors:**
- Events are limited to 1 per second for state changes
- Add delays between rapid state changes

## Example Output

```
I (1234) tv_example: ============================================
I (1234) tv_example:  SinricPro ESP-IDF TV Example
I (1234) tv_example:  Version: 1.0.0
I (1234) tv_example: ============================================
I (2345) tv_example: Connected to WiFi SSID:MyNetwork
I (3456) tv_example: ✓ Connected to SinricPro server
I (3456) tv_example: SinricPro started successfully!
I (5678) tv_example: PowerState: ON
I (5678) tv_example: Volume: 50
I (6789) tv_example: Input changed to: HDMI2
I (7890) tv_example: Channel: 5 - HBO
```

## See Also

- [SinricPro Documentation](https://help.sinric.pro)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/)
- [SinricPro Arduino Examples](https://github.com/sinricpro/esp8266-esp32-sdk)

## License

This example is licensed under Creative Commons Attribution-Share Alike 4.0 International License (CC BY-SA 4.0).
