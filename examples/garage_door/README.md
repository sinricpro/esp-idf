# SinricPro Garage Door Example

Control garage door with voice commands using relay and position sensors.

## Hardware

- ESP32 board
- Relay module (5V)
- 2x Magnetic reed switches (for open/closed detection)
- 2x Magnets (for reed switches)
- Garage door opener with button terminal

## Wiring

### Relay to Garage Door Opener

```
ESP32 GPIO18    → Relay IN
Relay VCC       → 5V (from external power or ESP32 VIN if <100mA)
Relay GND       → GND
Relay COM       → Garage Opener Terminal 1
Relay NO        → Garage Opener Terminal 2
```

Note: Connect relay in parallel with existing wall button.

### Reed Switches

```
ESP32 GPIO4     → Reed Switch 1 (Door Open Position)
Reed Switch 1   → GND

ESP32 GPIO5     → Reed Switch 2 (Door Closed Position)
Reed Switch 2   → GND

ESP32 GPIO0     → Manual Button (BOOT)
```

## Features

- Voice control to open/close garage door
- Position sensing (fully open/closed detection)
- Manual button control
- Status reporting to SinricPro
- Safety checks (prevents unnecessary triggers)

## Installation

1. **Reed Switch Placement**:
   - Mount one reed switch where door is fully OPEN
   - Mount one reed switch where door is fully CLOSED
   - Attach magnets to door in alignment with switches

2. **Relay Connection**:
   - Connect relay in parallel with wall button
   - Test by pressing manual button first

## Build

```bash
cd examples/garage_door
idf.py build flash monitor
```

## Configuration

Edit `main/garage_door_example.c` - update WiFi and SinricPro credentials.

For different GPIO pins, modify:
```c
#define RELAY_GPIO              (18)
#define REED_SWITCH_OPEN_GPIO   (4)
#define REED_SWITCH_CLOSED_GPIO (5)
#define BUTTON_GPIO             (0)
#define RELAY_PULSE_MS          (500)  /* Adjust for your opener */
```

## Voice Commands

- "Alexa, open the garage door"
- "Alexa, close the garage door"
- "Alexa, is the garage door open?"

## How It Works

1. **Opening/Closing**: ESP32 triggers relay momentarily (500ms pulse)
2. **Position Detection**: Reed switches detect fully open/closed positions
3. **Status Updates**: State changes sent to SinricPro automatically

## Safety Considerations

**IMPORTANT**: Garage doors are heavy and can cause serious injury.

1. **Test Thoroughly**: Test all safety features before regular use
2. **Keep Safety Eyes**: Don't bypass existing safety sensors
3. **Manual Override**: Ensure manual operation still works
4. **Obstruction Detection**: Garage opener should have built-in obstruction detection
5. **Child Safety**: Educate family about voice control

## Compatible Garage Door Openers

Most garage door openers work with momentary contact closure:
- Chamberlain/LiftMaster
- Genie
- Craftsman
- Ryobi
- Linear

**Check your opener's manual** for wiring terminals (usually labeled "WALL BUTTON" or similar).

## Alternative Without Reed Switches

If you don't want to install reed switches:

1. Remove reed switch GPIO configuration
2. Modify `get_door_state()` to track estimated state based on timing
3. Add configurable door travel time constant

This is less accurate but simpler to install.

## Advanced Features

Consider adding:
- **Ultrasonic sensor**: More reliable distance measurement
- **Auto-close timer**: Close door after N minutes
- **Open notification**: Alert if door left open
- **Integration**: Trigger outdoor lights when opening

## Troubleshooting

- **Door doesn't respond**: Check relay connection polarity (try swapping NO/NC)
- **Multiple triggers**: Adjust `RELAY_PULSE_MS` (try 300ms or 1000ms)
- **Wrong state**: Verify reed switch alignment with magnets
- **Relay clicking but no action**: Check garage opener wiring
- **Safety reversal**: Ensure door safety sensors are working

## Wiring Diagram

```
┌─────────────────────────────────────┐
│         Garage Door Opener          │
│  ┌──────────────────────────────┐   │
│  │ Wall Button Terminals        │   │
│  │  [ ]─────────────[ ]         │   │
│  └────┬─────────────┬────────────┘   │
│       │Relay Parallel│                │
└───────┼─────────────┼────────────────┘
        │             │
    ┌───┴─┐       ┌───┴─┐
    │ COM │       │  NO │  Relay Module
    └─────┘       └─────┘
```

## Legal Notice

Check local regulations regarding automated garage door openers. Some jurisdictions have specific requirements for safety features.
