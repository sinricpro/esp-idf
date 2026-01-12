# SinricPro Thermostat Example

Control HVAC system with voice commands - temperature, mode, and automatic climate control.

## Hardware

- ESP32 board
- 3x Relays (for Heat, Cool, Fan)
- DHT22 temperature/humidity sensor (optional - example uses simulation)
- 3x LEDs (optional - to visualize relay states)

## Wiring

```
ESP32 GPIO18 → Relay 1 (HEAT) → Heating system
ESP32 GPIO19 → Relay 2 (COOL) → Cooling system
ESP32 GPIO21 → Relay 3 (FAN)  → Fan
ESP32 GPIO4  → DHT22 Data pin (optional)
DHT22 VCC    → 3.3V
DHT22 GND    → GND
```

## Features

- 5 thermostat modes: AUTO, COOL, HEAT, ECO, OFF
- Target temperature control (10-35°C)
- Temperature adjustment (warmer/cooler)
- Current temperature/humidity reporting
- Automatic HVAC control with hysteresis
- Simulated DHT22 sensor (replace with real sensor)

## Voice Commands

- "Alexa, turn on the thermostat"
- "Alexa, set thermostat to 72 degrees"
- "Alexa, make it warmer"
- "Alexa, make it cooler"
- "Alexa, set thermostat to heat mode"
- "Alexa, set thermostat to cool mode"
- "Alexa, set thermostat to auto"

## Build

```bash
cd examples/thermostat
idf.py build flash monitor
```

## Configuration

Edit `main/thermostat_example.c` - update WiFi and SinricPro credentials.

For different GPIO pins or sensor, modify:
```c
#define RELAY_HEAT_GPIO     (18)
#define RELAY_COOL_GPIO     (19)
#define RELAY_FAN_GPIO      (21)
#define DHT_GPIO            (4)
```

## HVAC Logic

- **HEAT Mode**: Activates heating when current temp < target - hysteresis
- **COOL Mode**: Activates cooling when current temp > target + hysteresis
- **AUTO Mode**: Automatically switches between heating and cooling
- **ECO Mode**: Wider temperature range for energy savings
- **OFF Mode**: All HVAC systems disabled

Hysteresis: 0.5°C (prevents rapid on/off cycling)

## Real DHT22 Integration

To use a real DHT22 sensor, replace the simulated sensor task with DHT library:

1. Add DHT component to `main/idf_component.yml`:
```yaml
dependencies:
  espressif/esp-dht: "^1.0.0"
```

2. Replace `temperature_sensor_task()` with actual DHT22 readings.
