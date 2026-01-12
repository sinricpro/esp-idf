# SinricPro Smart Lock Example

Control a door lock with voice commands using a servo motor.

## Hardware

- ESP32 board
- Servo motor (SG90 or similar)
- External 5V power supply for servo (recommended)
- Status LED (optional - built-in LED used)

## Wiring

```
ESP32 GPIO18     → Servo Signal (Orange/Yellow)
External 5V      → Servo VCC (Red)
ESP32 GND + 5V GND → Servo GND (Brown/Black)
ESP32 GPIO2      → Status LED (built-in)
ESP32 GPIO0      → BOOT button (built-in)
```

**Important**: Servos can draw significant current. Use an external 5V power supply for the servo, not the ESP32's 3.3V pin.

## Features

- Lock/unlock control via voice
- Servo motor for physical lock mechanism
- Manual button control
- Status LED (on=unlocked, off=locked)
- Event reporting to SinricPro

## Voice Commands

- "Alexa, lock the door"
- "Alexa, unlock the door"
- "Alexa, is the door locked?"

## Build

```bash
cd examples/lock
idf.py build flash monitor
```

## Configuration

Edit `main/lock_example.c` - update WiFi and SinricPro credentials.

For different GPIO pins or servo angles, modify:
```c
#define SERVO_GPIO              (18)
#define STATUS_LED_GPIO         (2)
#define BUTTON_GPIO             (0)
#define SERVO_LOCKED_ANGLE      (0)    /* Adjust for your lock */
#define SERVO_UNLOCKED_ANGLE    (90)   /* Adjust for your lock */
```

## Servo Calibration

Different servos may require different angles. Test your servo:

1. Set `SERVO_LOCKED_ANGLE` to the angle where lock bolt is fully extended
2. Set `SERVO_UNLOCKED_ANGLE` to the angle where lock bolt is fully retracted
3. Typical ranges: 0-90° or 0-180° depending on your mechanism

## Physical Button

Press the BOOT button (GPIO0) to manually toggle lock state. The new state is sent to SinricPro.

## Security Note

This is a demonstration example. For production smart locks:
- Add tamper detection
- Add battery backup
- Add manual override mechanism
- Add audit logging
- Consider fail-secure vs fail-safe requirements
