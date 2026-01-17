# Power Sensor Example

This example demonstrates how to monitor electrical power consumption and report energy metrics to SinricPro.

## Features

- **Voltage Monitoring**: Measures AC voltage (V)
- **Current Monitoring**: Measures AC current (A)
- **Power Calculation**: Real power consumption (W)
- **Apparent Power**: Total power including reactive component (VA)
- **Reactive Power**: Non-productive power (VAR)
- **Power Factor**: Efficiency metric (0.0 to 1.0)
- **Real-time Dashboard**: View energy metrics in the SinricPro app
- **Periodic Updates**: Sends readings every 60 seconds

## Hardware Required

- ESP32 development board
- Power/Energy monitoring IC (PZEM-004T, ACS712, INA219, or similar)
  - Note: This example simulates sensor readings; integrate actual sensor for real use

## Power Monitoring IC Options

### Recommended ICs

1. **PZEM-004T**: Complete AC power monitor with UART interface
   - Measures: Voltage, Current, Power, Energy, Frequency, Power Factor
   - Easy to use, all-in-one solution

2. **ACS712**: Hall-effect current sensor
   - Measures: AC/DC current (5A, 20A, 30A versions)
   - Requires voltage measurement separately

3. **INA219**: High-side current/power monitor
   - Measures: DC voltage and current
   - I2C interface, ideal for DC loads

4. **HLW8012**: Single-phase energy metering IC
   - Used in Sonoff Pow devices
   - Measures: Voltage, Current, Power, Energy

## Typical Wiring (PZEM-004T)

| PZEM-004T | ESP32 |
|-----------|-------|
| VCC | 5V |
| GND | GND |
| TX | RX (GPIO 16) |
| RX | TX (GPIO 17) |

**⚠️ WARNING**: When measuring AC mains voltage (110V/230V), use proper isolation and safety precautions!

## Configuration

1. Update WiFi credentials in `power_sensor_example.c`:
   ```c
   #define WIFI_SSID      "your_wifi_ssid"
   #define WIFI_PASS      "your_wifi_password"
   ```

2. Set nominal voltage for your region:
   ```c
   #define NOMINAL_VOLTAGE  (230.0f)  /* 230V for EU, 120V for US */
   ```

3. Set up a Power Sensor device on [SinricPro](https://sinric.pro):
   - Create a new Power Sensor device
   - Copy the Device ID, App Key, and App Secret
   - Update in `power_sensor_example.c`:
     ```c
     #define DEVICE_ID      "your_device_id"
     #define APP_KEY        "your_app_key"
     #define APP_SECRET     "your_app_secret"
     ```

## Building and Flashing

```bash
cd examples/power_sensor
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Power Metrics Explained

### Real Power (P) - Watts (W)
- Actual power consumed by the load
- Performs useful work

### Apparent Power (S) - Volt-Amperes (VA)
- Total power supplied by the source
- `S = V × I`

### Reactive Power (Q) - Volt-Amperes Reactive (VAR)
- Power that oscillates between source and load
- Does not perform useful work
- `Q = √(S² - P²)`

### Power Factor (PF)
- Efficiency of power usage
- `PF = P / S`
- Range: 0.0 (poor) to 1.0 (ideal)
- Good: PF ≥ 0.9
- Typical household: PF = 0.7 to 0.95

## Example Readings

```
Power Readings:
  Voltage: 228.4V
  Current: 4.32A
  Power: 850.2W
  Apparent Power: 986.5VA
  Reactive Power: 445.8VAR
  Power Factor: 0.86
```

## How It Works

1. The ESP32 connects to WiFi
2. Establishes a secure WebSocket connection to SinricPro
3. Reads power metrics from the sensor (simulated in this example)
4. Calculates derived values (apparent power, reactive power)
5. Sends readings to SinricPro dashboard every 60 seconds

## Use Cases

- **Energy Monitoring**: Track power consumption of appliances
- **Home Automation**: Control devices based on power usage
- **Cost Calculation**: Monitor electricity costs in real-time
- **Anomaly Detection**: Alert on unusual power patterns
- **Solar Monitoring**: Track solar panel output

## Safety Notice

⚠️ **IMPORTANT SAFETY WARNINGS**:

1. **AC Mains Voltage is DANGEROUS** - Can cause serious injury or death
2. Always use **proper isolation** when measuring AC mains
3. Use **certified power monitoring modules** with built-in safety features
4. **Never** directly connect ESP32 GPIO pins to AC mains
5. Follow local electrical codes and regulations
6. If unsure, consult a licensed electrician

For low-voltage DC applications (≤60V), safety requirements are less stringent.

## View Your Data

After the device is running:
1. Open the SinricPro app on your phone
2. Navigate to your Power Sensor device
3. View real-time voltage, current, power, and power factor
4. Check historical data and energy consumption trends
5. Set up alerts for abnormal power usage
