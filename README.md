# SinricPro ESP-IDF Component

[![Platform](https://img.shields.io/badge/platform-ESP--IDF-blue.svg)](https://docs.espressif.com/projects/esp-idf/)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v4.4%2B%20%7C%20v5.x-green.svg)](https://github.com/espressif/esp-idf)
[![License](https://img.shields.io/badge/license-CC%20BY--SA%204.0-orange.svg)](LICENSE)

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

**Phase 1 (Current):**
- ✅ Smart Switch - On/Off control

**Coming Soon:**
- 🔜 Smart Light - RGB color, brightness, color temperature
- 🔜 Thermostat - Temperature control
- 🔜 Fan - Speed control
- 🔜 Lock - Lock/unlock control
- 🔜 Sensors - Motion, contact, temperature, etc.

## Requirements

- ESP-IDF v4.4 or higher. Tested on ESP-IDF 6.1
- ESP32, ESP32-S2, ESP32-S3, ESP32-C3, or ESP32-C6
- SinricPro account ([sign up free](https://sinric.pro))

## Installation

### Method 1: ESP Component Registry (Recommended)

Add to your project's `idf_component.yml`:

```yaml
dependencies:
  sinricpro/esp-idf: "^1.0.0"
```

### Method 2: Manual Installation

```bash
cd your_project/components
git clone https://github.com/sinricpro/esp-idf.git sinricpro
```

Or

```bash
idf.py add-dependency "sinricpro/esp-idf^1.0.1"
```

View at: https://components.espressif.com/components/sinricpro/esp-idf

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

## License

Copyright (c) 2019-2025 Sinric. All rights reserved.

This component is licensed under the **Creative Commons Attribution-Share Alike 4.0 International License (CC BY-SA 4.0)**.

You are free to:
- Share and adapt this work for any purpose (including commercially)

Under the following terms:
- Give appropriate credit
- Distribute your contributions under the same license

See [LICENSE](LICENSE) file for details.

## Support

- **Documentation**: [help.sinric.pro](https://help.sinric.pro)
- **Community**: [Discord](https://discord.gg/W5299EgB59)
- **Issues**: [GitHub Issues](https://github.com/sinricpro/esp-idf/issues)
- **Email**: support@sinric.pro

## Related Projects

- [SinricPro Arduino SDK](https://github.com/sinricpro/esp8266-esp32-sdk)
- [SinricPro Python SDK](https://github.com/sinricpro/python-sdk)
- [SinricPro Node.js SDK](https://github.com/sinricpro/nodejs-sdk)


---

Made with ❤️ by the SinricPro Team
