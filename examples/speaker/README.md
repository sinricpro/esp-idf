# SinricPro Speaker Example

This example demonstrates how to control a smart speaker using SinricPro with voice commands via Alexa or Google Home.

## Features

This example showcases all Speaker capabilities:

- **Power Control** - Turn speaker on/off
- **Volume Control** - Set or adjust volume (0-100)
- **Mute Control** - Mute/unmute audio
- **Media Control** - Play, pause, stop, next, previous
- **Input Selection** - Switch between AUX, Bluetooth, WiFi, etc.
- **Equalizer Control** - Adjust bass, midrange, treble (-10 to +10)
- **Mode Control** - Switch between MOVIE, MUSIC, NIGHT, SPORT, TV modes
- **Settings** - Configure speaker settings

## Hardware Required

- ESP32, ESP32-S2, ESP32-S3, ESP32-C3, or ESP32-C6 development board
- USB cable for power supply and programming
- (Optional) LED connected to GPIO2 to visualize speaker state
- Built-in BOOT button (GPIO0) for manual control

## How to Use

### 1. Setup SinricPro Account

1. Create a free account at [sinric.pro](https://sinric.pro)
2. Go to "Devices" and click "Add Device"
3. Select device type: **Speaker**
4. Give it a name (e.g., "Living Room Speaker")
5. Note down the **Device ID**, **App Key**, and **App Secret**

### 2. Configure the Example

Edit `main/speaker_example.c` and update the following:

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
cd examples/speaker
idf.py set-target esp32  # or esp32s2, esp32s3, esp32c3, esp32c6
idf.py build
idf.py flash monitor
```

### 4. Voice Commands

Once the device is online, you can control it with voice commands:

**Power Control:**
- "Alexa, turn on the speaker"
- "Alexa, turn off the speaker"

**Volume Control:**
- "Alexa, set speaker volume to 50"
- "Alexa, increase speaker volume"
- "Alexa, decrease speaker volume by 10"
- "Alexa, volume up"
- "Alexa, volume down"

**Mute Control:**
- "Alexa, mute the speaker"
- "Alexa, unmute the speaker"

**Media Control:**
- "Alexa, play"
- "Alexa, pause"
- "Alexa, stop"
- "Alexa, next"
- "Alexa, previous"

**Input Selection:**
- "Alexa, change speaker input to Bluetooth"
- "Alexa, switch to AUX"

**Equalizer Control:**
- "Alexa, increase bass"
- "Alexa, decrease treble"
- "Alexa, set bass to 5"

**Mode Control:**
- "Alexa, set speaker mode to MUSIC"
- "Alexa, change mode to MOVIE"
- Supported modes: MOVIE, MUSIC, NIGHT, SPORT, TV

## Physical Button Control

Press the built-in BOOT button (GPIO0) to toggle the speaker power state. The built-in LED (GPIO2) will indicate the current state.

## Customization

### Modify GPIO Pins

Change the LED and button GPIO pins in `main/speaker_example.c`:

```c
#define LED_GPIO       GPIO_NUM_2   /* Your LED GPIO */
#define BUTTON_GPIO    GPIO_NUM_0   /* Your button GPIO */
```

### Implement Audio DSP

Add your audio processing logic to the equalizer callback:

```c
static bool on_equalizer(const char *device_id,
                         sinricpro_speaker_equalizer_bands_t *bands,
                         void *user_data)
{
    /* Apply equalizer settings to your audio DSP */
    audio_dsp_set_bass(bands->bass);      /* -10 to +10 */
    audio_dsp_set_midrange(bands->midrange);
    audio_dsp_set_treble(bands->treble);

    speaker_equalizer = *bands;
    return true;
}
```

### Implement Mode Switching

Add logic to switch between audio profiles:

```c
static bool on_mode(const char *device_id, const char **mode, void *user_data)
{
    if (strcmp(*mode, "MUSIC") == 0) {
        /* Optimize for music playback */
    } else if (strcmp(*mode, "MOVIE") == 0) {
        /* Optimize for movie audio */
    } else if (strcmp(*mode, "NIGHT") == 0) {
        /* Reduce volume, enhance voice clarity */
    }
    return true;
}
```

### Send Speaker Events

Report speaker state changes to SinricPro:

```c
/* Volume changed */
sinricpro_speaker_send_volume_event(my_speaker, 80, SINRICPRO_CAUSE_PHYSICAL_INTERACTION);

/* Equalizer changed */
sinricpro_speaker_equalizer_bands_t bands = {3, 0, -2};  /* Bass +3, Treble -2 */
sinricpro_speaker_send_equalizer_event(my_speaker, &bands, SINRICPRO_CAUSE_PHYSICAL_INTERACTION);

/* Mode changed */
sinricpro_speaker_send_mode_event(my_speaker, "MUSIC", SINRICPRO_CAUSE_PHYSICAL_INTERACTION);
```

## Integration Examples

### Connect to I2S Audio

```c
#include "driver/i2s.h"

/* Configure I2S for audio output */
static void audio_init(void)
{
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = 26,
        .ws_io_num = 25,
        .data_out_num = 22,
        .data_in_num = I2S_PIN_NO_CHANGE,
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

/* Control volume via I2S */
static bool on_volume(const char *device_id, int *volume, void *user_data)
{
    speaker_volume = *volume;
    /* Adjust I2S DAC output or use external DAC */
    set_dac_volume(speaker_volume);
    return true;
}
```

### Bluetooth Audio Integration

```c
#include "esp_a2dp_api.h"

static bool on_media_control(const char *device_id, const char *control, void *user_data)
{
    if (strcmp(control, "Play") == 0) {
        esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
    } else if (strcmp(control, "Pause") == 0) {
        esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
    } else if (strcmp(control, "Next") == 0) {
        esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SKIP_FORWARD);
    } else if (strcmp(control, "Previous") == 0) {
        esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SKIP_BACKWARD);
    }
    return true;
}
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

**Equalizer values out of range:**
- Valid range is -10 to +10 for each band
- Values outside this range will be clamped

## Example Output

```
I (1234) speaker_example: ============================================
I (1234) speaker_example:  SinricPro ESP-IDF Speaker Example
I (1234) speaker_example:  Version: 1.0.0
I (1234) speaker_example: ============================================
I (2345) speaker_example: Connected to WiFi SSID:MyNetwork
I (3456) speaker_example: ✓ Connected to SinricPro server
I (3456) speaker_example: SinricPro started successfully!
I (5678) speaker_example: PowerState: ON
I (5678) speaker_example: Volume: 75
I (6789) speaker_example: Equalizer: Bass=3, Midrange=0, Treble=-2
I (7890) speaker_example: Mode changed to: MUSIC
I (8901) speaker_example: Media control: Play
```

## See Also

- [SinricPro Documentation](https://help.sinric.pro)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/)
- [ESP-IDF I2S Driver](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/i2s.html)
- [SinricPro Arduino Examples](https://github.com/sinricpro/esp8266-esp32-sdk)

## License

This example is licensed under Creative Commons Attribution-Share Alike 4.0 International License (CC BY-SA 4.0).
