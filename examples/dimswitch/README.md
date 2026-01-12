# SinricPro DimSwitch Example

Control a dimmable light with voice commands - brightness control with PWM.

## Hardware

- ESP32 board
- High-power LED or LED strip
- MOSFET or LED driver (e.g., IRF520, IRLZ44N)
- Current-limiting resistor (if using single LED)
- External power supply (12V for LED strip)

## Wiring

### For LED Strip (12V)

```
ESP32 GPIO25      → MOSFET Gate
MOSFET Source     → GND (common ground with ESP32)
MOSFET Drain      → LED Strip Negative (-)
LED Strip (+)     → 12V Power Supply (+)
Power Supply (-)  → GND (common ground)
```

### For Single High-Power LED

```
ESP32 GPIO25      → MOSFET Gate
MOSFET Source     → GND
MOSFET Drain      → LED Cathode (-)
LED Anode (+)     → Current Resistor → 12V+
```

## Features

- Dimmable brightness 0-100%
- PWM-based LED control (5kHz)
- Brightness adjustment (increase/decrease)
- Physical button for on/off toggle
- Smooth dimming transitions
- Auto turn-on when brightness is set

## Voice Commands

- "Alexa, turn on the light"
- "Alexa, turn off the light"
- "Alexa, dim the light"
- "Alexa, brighten the light"
- "Alexa, set light to 50 percent"
- "Alexa, set light brightness to 75"

## Build

```bash
cd examples/dimswitch
idf.py build flash monitor
```

## Configuration

Edit `main/dimswitch_example.c` - update WiFi and SinricPro credentials.

For different GPIO or PWM settings, modify:
```c
#define LED_GPIO                (25)
#define LED_FREQUENCY           (5000)  /* 5 kHz */
#define BUTTON_GPIO             (0)
```

## Physical Button

Press BOOT button (GPIO0) to toggle light on/off. The state is sent to SinricPro.

## DimSwitch vs Light Device

- **DimSwitch**: Simple brightness control only (this example)
- **Light**: Adds color control, color temperature (see `examples/light`)

Choose DimSwitch for:
- Single-color LED strips
- Dimmable white bulbs
- Simple lamp control

Choose Light for:
- RGB LED strips
- Color-changing bulbs
- Color temperature control

## Hardware Notes

1. **PWM Frequency**: 5kHz is flicker-free for most LEDs
2. **MOSFET**: Use logic-level MOSFETs (gate threshold < 3.3V)
3. **Heat Sinking**: Use heatsink for high-power applications
4. **Current Limit**: Calculate resistor: R = (Vsupply - Vled) / Iled
5. **Common Ground**: ESP32 and LED power supply must share GND

## Recommended MOSFETs

- **IRF520**: Good for 5-10A loads
- **IRLZ44N**: Logic-level, handles up to 47A
- **IRL540N**: Logic-level, handles up to 33A

## Safety

- Never exceed MOSFET current rating
- Add proper heat dissipation for continuous operation
- Use appropriate gauge wire for high currents
- Fuse the power supply for protection
