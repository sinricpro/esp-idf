# SinricPro Fan Example

Control a DC fan with voice commands - variable speed control with PWM.

## Hardware

- ESP32 board
- DC fan (5V or 12V)
- MOSFET or motor driver (e.g., L293D, L298N, IRF520)
- External power supply matching fan voltage
- Flyback diode (1N4007 or similar)

## Wiring

### Using MOSFET (IRF520)

```
ESP32 GPIO25      → MOSFET Gate
MOSFET Source     → GND (common ground with ESP32)
MOSFET Drain      → Fan Negative (-)
Fan Positive (+)  → External Power Supply (+)
Power Supply (-)  → GND (common ground)
Flyback Diode     → Across fan terminals (cathode to +)
```

### Using L298N Motor Driver

```
ESP32 GPIO25 → IN1 (PWM input)
ESP32 GND    → GND
OUT1/OUT2    → Fan
12V/5V       → External power supply
```

## Features

- Variable speed control 0-100%
- PWM-based motor control (25kHz)
- Speed adjustment (increase/decrease)
- Physical button for preset cycling
- Smooth speed transitions

## Voice Commands

- "Alexa, turn on the fan"
- "Alexa, turn off the fan"
- "Alexa, set fan speed to 75"
- "Alexa, set fan to maximum"
- "Alexa, increase fan speed"
- "Alexa, decrease fan speed"

## Build

```bash
cd examples/fan
idf.py build flash monitor
```

## Configuration

Edit `main/fan_example.c` - update WiFi and SinricPro credentials.

For different GPIO or PWM settings, modify:
```c
#define FAN_GPIO                (25)
#define FAN_FREQUENCY           (25000)  /* 25 kHz */
#define BUTTON_GPIO             (0)
```

## Speed Presets

Press BOOT button to cycle through:
- OFF (0%)
- LOW (33%)
- MEDIUM (66%)
- HIGH (100%)

## Hardware Notes

1. **MOSFET Selection**: Use logic-level MOSFETs (IRF520, IRLZ44N) for 3.3V GPIO
2. **Flyback Diode**: REQUIRED to protect against inductive kickback
3. **Common Ground**: ESP32 and fan power supply must share common ground
4. **PWM Frequency**: 25kHz is above audible range (prevents buzzing)
5. **Minimum Speed**: Code sets minimum 20% duty for fan startup

## Safety

- Never power high-current devices directly from ESP32 pins
- Always use appropriate motor driver or MOSFET
- Add flyback diode for inductive loads
- Use proper heat sinking for high-power applications
