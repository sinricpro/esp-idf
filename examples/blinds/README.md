# SinricPro Motorized Blinds Example

Control window blinds/curtains with voice commands using DC motor or stepper motor.

## Hardware

- ESP32 board
- DC motor or stepper motor
- Motor driver (L298N, L293D, or DRV8825 for stepper)
- External power supply (12V for most motors)
- Manual control buttons (optional)

## Wiring

### Using L298N Motor Driver

```
ESP32 GPIO25 → IN1 (Motor direction)
ESP32 GPIO26 → IN2 (Motor direction)
ESP32 GPIO27 → ENA (Motor enable)
ESP32 GND    → GND

L298N OUT1   → Motor +
L298N OUT2   → Motor -
L298N +12V   → External 12V supply
L298N GND    → Power supply GND + ESP32 GND (common ground)

Buttons:
ESP32 GPIO32 → UP button → GND
ESP32 GPIO33 → DOWN button → GND
ESP32 GPIO0  → STOP button (BOOT) → GND
```

## Features

- Variable position control 0-100% (0=closed, 100=open)
- Automatic motor control with position tracking
- Manual buttons (up/down/stop)
- Smooth movement to target position
- Position reporting to SinricPro

## Build

```bash
cd examples/blinds
idf.py build flash monitor
```

## Configuration

Edit `main/blinds_example.c` - update WiFi and SinricPro credentials.

**IMPORTANT**: Calibrate travel time for your blinds:
```c
#define FULL_TRAVEL_TIME_MS (10000)  /* Adjust: Time from fully closed to fully open */
```

To calibrate:
1. Measure time for full open→close cycle
2. Set FULL_TRAVEL_TIME_MS to this value
3. Test accuracy, adjust if needed

## Voice Commands

- "Alexa, open the blinds"
- "Alexa, close the blinds"
- "Alexa, set blinds to 50 percent"
- "Alexa, open the blinds halfway"

## Motor Control Methods

### Method 1: DC Motor (Simple)
- Pros: Simple, cheap
- Cons: No position feedback, relies on timing
- Use with: L298N, L293D
- This example uses timing-based position estimation

### Method 2: Stepper Motor (Accurate)
- Pros: Precise position control, no drift
- Cons: More expensive, noisier
- Use with: DRV8825, A4988
- Modify code to use step counting instead of timing

### Method 3: Servo Motor
- Pros: Built-in position control
- Cons: Limited rotation (180° max)
- Use for: Small blinds, shutters
- See examples/lock for servo code

## Position Tracking

This example uses **time-based estimation**:
- Tracks motor runtime to estimate position
- Works well for consistent loads
- May drift over time (periodic recalibration recommended)

For better accuracy, add:
- Limit switches (detect fully open/closed)
- Rotary encoder (measure actual rotation)
- Current sensor (detect motor stall)

## Safety Features

**IMPORTANT**: Motorized blinds can be dangerous.

1. **Obstruction Detection**: Add current sensing to detect obstructions
2. **Limit Switches**: Prevent over-travel
3. **Thermal Protection**: Motor drivers can overheat
4. **Manual Override**: Always provide manual operation method

## Manual Buttons

- **UP**: Move to fully open (100%)
- **DOWN**: Move to fully closed (0%)
- **STOP**: Stop at current position

## Installation Tips

1. **Motor Selection**:
   - Calculate blind weight
   - Choose motor with 2-3x torque margin
   - Consider gearing for heavy blinds

2. **Mounting**:
   - Secure motor to wall/ceiling
   - Ensure smooth pulley/cord system
   - Test for binding or friction

3. **Power Supply**:
   - Size for motor current + margin
   - Use adequate gauge wire
   - Common ground is essential

## Troubleshooting

- **Motor doesn't move**: Check power supply, wiring polarity
- **Wrong direction**: Swap IN1/IN2 connections
- **Position drift**: Recalibrate FULL_TRAVEL_TIME_MS
- **Erratic movement**: Check common ground connection
- **Motor stalls**: Increase voltage or reduce load

## Advanced Features

Consider adding:

1. **Scheduling**: Open at sunrise, close at sunset
2. **Light sensor**: Automatic control based on sunlight
3. **Group control**: Control multiple blinds together
4. **Privacy mode**: Automatically close at night

## Alternative Hardware

This example can be adapted for:
- **Curtains**: Horizontal track systems
- **Roller shades**: Tube motor systems
- **Venetian blinds**: Tilt angle control
- **Shutters**: Open/close control

## Limit Switch Integration

Add limit switches for accurate calibration:

```c
#define LIMIT_OPEN_GPIO   (34)
#define LIMIT_CLOSED_GPIO (35)

void check_limit_switches() {
    if (!gpio_get_level(LIMIT_CLOSED_GPIO)) {
        blinds_position = 0;  /* Recalibrate at closed */
        set_motor_direction(MOTOR_STOP);
    }
    if (!gpio_get_level(LIMIT_OPEN_GPIO)) {
        blinds_position = 100;  /* Recalibrate at open */
        set_motor_direction(MOTOR_STOP);
    }
}
```

## Power Consumption

Typical DC motor blinds:
- Motor: 1-3A @ 12V during movement
- Idle: <50mA
- ESP32: ~150mA

Use appropriate power supply (recommend 12V 3A minimum).
