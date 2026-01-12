# SinricPro ESP-IDF Component

[![Platform](https://img.shields.io/badge/platform-ESP--IDF-blue.svg)](https://docs.espressif.com/projects/esp-idf/)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v4.4%2B%20%7C%20v5.x-green.svg)](https://github.com/espressif/esp-idf)
[![License](https://img.shields.io/badge/license-CC%20BY--SA%204.0-orange.svg)](LICENSE)
[![Build Status](https://github.com/sinricpro/esp-idf/workflows/Build%20and%20Test/badge.svg)](https://github.com/sinricpro/esp-idf/actions)
[![Examples](https://img.shields.io/badge/examples-13-brightgreen.svg)](examples/)

Official ESP-IDF component for [SinricPro](https://sinric.pro) - Control your ESP32 IoT devices with Alexa and Google Home.

## Features

- ✅ **Voice Control** - Works with Alexa and Google Home
- ✅ **Real-time** - WebSocket-based bidirectional communication
- ✅ **Secure** - HMAC-SHA256 message signatures
- ✅ **Reliable** - Auto-reconnection and heartbeat monitoring
- ✅ **Event-driven** - ESP event loop integration
- ✅ **Type Safe** - Full C API with optional C++ wrappers
- ✅ **Configurable** - Kconfig integration
- ✅ **Cross-Platform** - ESP-IDF v4.4+ and v5.x support

## Supported Devices

All devices below have complete API support and working examples:

### Simple Devices
- ✅ **Switch** - Basic on/off control with physical button
- ✅ **Motion Sensor** - PIR motion detection
- ✅ **Contact Sensor** - Door/window reed switch detection

### Lighting
- ✅ **Smart Light** - RGB color, brightness, color temperature (2200-7000K)
- ✅ **DimSwitch** - Dimmable switch with brightness control (0-100%)

### Climate Control
- ✅ **Thermostat** - HVAC control with AUTO/COOL/HEAT/ECO/OFF modes
- ✅ **Fan** - Variable speed control (0-100%)
- ✅ **Temperature Sensor** - DHT22/DHT11 temperature and humidity

### Security & Access
- ✅ **Lock** - Servo-based lock/unlock control
- ✅ **Garage Door** - Relay control with position sensors

### Home Automation
- ✅ **Blinds** - Motorized blinds/curtains with position control (0-100%)

### Media Devices
- ✅ **TV** - Volume, mute, media control, input selection, channels
- ✅ **Speaker** - Volume, mute, media control, equalizer, modes

### Additional Devices (API Only)
- ✅ Air Quality Sensor - PM1, PM2.5, PM10 measurements
- ✅ Power Sensor - Voltage, current, power monitoring
- ✅ Window AC - Air conditioner with fan speed and temperature

## Requirements

- ESP-IDF v4.4 or higher. Tested on ESP-IDF 6.1
- ESP32, ESP32-S2, ESP32-S3, ESP32-C3, or ESP32-C6
- SinricPro account ([sign up free](https://sinric.pro))

## Installation

### Method 1: ESP Component Registry (Recommended)

Add to your project's `idf_component.yml`:

```yaml
dependencies:
  sinricpro/esp-idf: "^1.1.1"
```

### Method 2: Manual Installation

```bash
cd your_project/components
git clone https://github.com/sinricpro/esp-idf.git sinricpro
```

Or

```bash
idf.py add-dependency "sinricpro/esp-idf^1.1.1"
```

View at: https://components.espressif.com/components/sinricpro/esp-idf

## Examples

The component includes **13 complete working examples** demonstrating all device types and features:

| Example | Description | Complexity | Hardware |
|---------|-------------|------------|----------|
| [Switch](examples/switch/) | Basic on/off control | ⭐ Simple | LED, Button |
| [Light](examples/light/) | RGB LED with color & brightness | ⭐⭐⭐ Complex | RGB LED, PWM |
| [DimSwitch](examples/dimswitch/) | Dimmable light control | ⭐⭐ Medium | LED, PWM |
| [Motion Sensor](examples/motion_sensor/) | PIR motion detection | ⭐ Simple | PIR sensor |
| [Contact Sensor](examples/contact_sensor/) | Door/window detection | ⭐ Simple | Reed switch |
| [Temperature Sensor](examples/temperature_sensor/) | Temp & humidity monitoring | ⭐⭐ Medium | DHT22/DHT11 |
| [Thermostat](examples/thermostat/) | HVAC climate control | ⭐⭐⭐ Complex | Relays, DHT22 |
| [Lock](examples/lock/) | Smart lock control | ⭐⭐ Medium | Servo motor |
| [Garage Door](examples/garage_door/) | Garage door automation | ⭐⭐⭐ Complex | Relay, Reed switches |
| [Fan](examples/fan/) | Variable speed fan | ⭐⭐ Medium | DC motor, PWM |
| [Blinds](examples/blinds/) | Motorized blinds/curtains | ⭐⭐⭐ Complex | DC motor, L298N |
| [TV](examples/tv/) | Media control & channels | ⭐⭐⭐ Complex | Simulated/IR |
| [Speaker](examples/speaker/) | Audio control & equalizer | ⭐⭐⭐ Complex | Simulated |

Each example includes:
- Complete working code
- Wiring diagrams
- Voice command examples
- Hardware requirements
- Step-by-step setup guide
 
## Quick Start

### 1. Get SinricPro Credentials

1. Sign up at [sinric.pro](https://sinric.pro)
2. Create a new Switch device
3. Note your `APP_KEY`, `APP_SECRET`, and `DEVICE_ID`

### 2. Minimal Example

```c
#include "sinricpro.h"
#include "sinricpro_switch.h"

#define DEVICE_ID      "YOUR_DEVICE_ID"
#define APP_KEY        "YOUR_APP_KEY"
#define APP_SECRET     "YOUR_APP_SECRET"

/* PowerState callback */
static bool on_power_state(const char *device_id, bool *state, void *user_data)
{
    printf("Device %s\n", *state ? "ON" : "OFF");
    gpio_set_level(GPIO_NUM_2, *state);  /* Control LED */
    return true;  /* Return true for success */
}

void app_main(void)
{
    /* Initialize WiFi (code omitted for brevity) */

    /* Configure SinricPro */
    sinricpro_config_t config = {
        .app_key = APP_KEY,
        .app_secret = APP_SECRET,
        .auto_reconnect = true,
    };

    /* Initialize */
    sinricpro_init(&config);

    /* Create switch device */
    sinricpro_device_handle_t my_switch = sinricpro_switch_create(DEVICE_ID);

    /* Register callback */
    sinricpro_switch_on_power_state(my_switch, on_power_state, NULL);

    /* Start */
    sinricpro_start();

    /* Send event (e.g., button press) */
    sinricpro_switch_send_power_state_event(my_switch, true,
                                             SINRICPRO_CAUSE_PHYSICAL_INTERACTION);
}
```

### 3. Build and Flash

```bash
idf.py build
idf.py flash monitor
```

### Reporting Issues
- Check [existing issues](https://github.com/sinricpro/esp-idf/issues)
- Include ESP-IDF version, chip type, and example name
- Provide minimal reproduction code
 
## Support

- **Documentation**: [help.sinric.pro](https://help.sinric.pro)
- **Community**: [Discord](https://discord.gg/W5299EgB59) - Get help from the community
- **Issues**: [GitHub Issues](https://github.com/sinricpro/esp-idf/issues) - Report bugs or request features
- **Email**: support@sinric.pro - Direct support
- **Examples**: All examples include detailed README files with troubleshooting

## Related Projects

- [SinricPro Arduino SDK](https://github.com/sinricpro/esp8266-esp32-sdk)
- [SinricPro Python SDK](https://github.com/sinricpro/python-sdk)
- [SinricPro Node.js SDK](https://github.com/sinricpro/nodejs-sdk)


---

Made with ❤️ by the SinricPro Team
