# SinricPro Temperature Sensor Example

Monitor temperature and humidity with DHT22/DHT11 sensor and report to SinricPro.

## Hardware

- ESP32 board
- DHT22 (AM2302) or DHT11 sensor
- 10kΩ pull-up resistor (often included on sensor modules)

## Wiring

### DHT22/DHT11 Sensor

```
DHT22 Pin 1 (VCC)  → ESP32 3.3V
DHT22 Pin 2 (DATA) → ESP32 GPIO4 + 10kΩ pull-up to 3.3V
DHT22 Pin 3 (NC)   → Not connected
DHT22 Pin 4 (GND)  → ESP32 GND
```

Note: Many DHT modules have only 3 pins with built-in pull-up resistor.

## Features

- Temperature monitoring (-40°C to 80°C for DHT22)
- Humidity monitoring (0-100%)
- Periodic reporting (every 60 seconds)
- Comfort level indicator
- Simulated sensor for testing (no hardware needed)

## Build

```bash
cd examples/temperature_sensor
idf.py build flash monitor
```

## Configuration

Edit `main/temperature_sensor_example.c` - update WiFi and SinricPro credentials.

For different GPIO pin, modify:
```c
#define DHT_GPIO                (4)
```

## Using Real DHT22 Sensor

The example uses simulated sensor by default. To use real DHT22:

1. Edit `main/idf_component.yml` and uncomment:
```yaml
dependencies:
  espressif/esp-dht:
    version: "^1.0.0"
```

2. Edit `main/temperature_sensor_example.c` and set:
```c
#define USE_SIMULATED_SENSOR   0
```

3. Implement `read_dht22_real()` function using the DHT library.

## Voice Queries

- "Alexa, what's the temperature?"
- "Alexa, what's the humidity?"
- "Alexa, what's the temperature in the living room?"

Note: Temperature sensors are read-only devices (monitoring only).

## Comfort Levels

The example provides comfort indicators:
- **TOO COLD**: < 18°C
- **TOO HOT**: > 26°C
- **TOO DRY**: < 30% humidity
- **TOO HUMID**: > 60% humidity
- **OPTIMAL**: 18-26°C with 30-60% humidity

## Applications

- **Home Automation**: Trigger AC/heater based on temperature
- **Greenhouse**: Monitor plant growing conditions
- **Server Room**: Overheat alerts
- **Smart Home**: Multi-room climate monitoring
- **Baby Monitor**: Nursery temperature alerts

## DHT Sensor Comparison

| Feature | DHT11 | DHT22 (AM2302) |
|---------|-------|----------------|
| Temperature Range | 0-50°C | -40 to 80°C |
| Temperature Accuracy | ±2°C | ±0.5°C |
| Humidity Range | 20-80% | 0-100% |
| Humidity Accuracy | ±5% | ±2-5% |
| Sampling Rate | 1 Hz | 0.5 Hz |
| Price | $ | $$ |

**Recommendation**: Use DHT22 for better accuracy and wider range.

## Rate Limiting

SinricPro limits sensor events to 1 per 60 seconds. The example respects this limit by updating every 60 seconds.

## Alternative Sensors

This example can be adapted for other sensors:
- **BME280**: Temperature, humidity, pressure (I2C)
- **DS18B20**: High-accuracy temperature (1-Wire)
- **SHT31**: High-accuracy temp/humidity (I2C)
- **AHT20**: Compact temp/humidity (I2C)

## Troubleshooting

- **All zeros**: Check wiring, ensure 3.3V power
- **Timeout errors**: Add/check pull-up resistor
- **Fluctuating values**: Poor connection or interference
- **DHT not found**: Verify GPIO pin number
