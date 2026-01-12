# SinricPro Contact Sensor Example

Monitor door/window status with magnetic reed switch sensor.

## Hardware

- ESP32 board
- Magnetic reed switch (normally open)
- 10kΩ pull-up resistor (optional - uses internal pull-up)
- Magnet

## Wiring

### Reed Switch Wiring

```
ESP32 GPIO4     → Reed Switch Pin 1
Reed Switch Pin 2 → GND
ESP32 GPIO2     → Status LED (built-in)
```

Note: Internal pull-up resistor is enabled, so external resistor is optional.

### How Reed Switches Work

- **Normally Open (NO)**: Switch is OPEN when magnet is away
- **Closes**: Switch CLOSES when magnet is near
- **With Pull-up**:
  - Magnet near (closed) → GPIO reads LOW (0)
  - Magnet away (open) → GPIO reads HIGH (1)

## Features

- Detects door/window open/close events
- Sends contact state to SinricPro
- Status LED indicates contact state
- Rate limiting (1 event per 60 seconds)
- Low power consumption

## Installation

Mount reed switch on door frame, magnet on door:

```
┌─────────────┐
│   FRAME     │  ← Reed switch
│             │
│   ┌─────┐   │
│   │DOOR │   │  ← Magnet
│   └─────┘   │
└─────────────┘

When door is CLOSED: Magnet aligns with reed switch → Contact detected
When door is OPEN: Magnet moves away → Contact broken
```

## Build

```bash
cd examples/contact_sensor
idf.py build flash monitor
```

## Configuration

Edit `main/contact_sensor_example.c` - update WiFi and SinricPro credentials.

For different GPIO pins, modify:
```c
#define REED_SWITCH_GPIO    (4)
#define STATUS_LED_GPIO     (2)
```

## Notifications

Configure in SinricPro portal to receive notifications:
- "Front door opened"
- "Window closed"
- Custom messages for each sensor

## Voice Queries

- "Alexa, is the front door open?"
- "Alexa, is the garage door closed?"

Note: Contact sensors are read-only devices (no control, only monitoring).

## Applications

- **Security**: Door/window intrusion detection
- **Automation**: Turn on lights when door opens
- **Monitoring**: Track mailbox, cabinet, safe access
- **Energy**: Close windows reminder when AC is on

## Hardware Notes

1. **Reed Switch Types**:
   - Normally Open (NO) - most common
   - Normally Closed (NC) - invert logic in code

2. **Magnet Alignment**:
   - Maximum gap: ~15mm for standard switches
   - Align switch and magnet in same line

3. **Wire Length**:
   - Reed switches work with long wires (10m+)
   - Consider twisted pair for noise immunity

4. **Mounting**:
   - Use epoxy or double-sided tape
   - Keep away from ferrous metal surfaces

## Rate Limiting

SinricPro limits contact sensor events to 1 per 60 seconds. Rapid open/close cycles will be rate-limited. This is normal and prevents server overload.

## Troubleshooting

- **No events**: Check magnet alignment, try moving closer
- **Inverted logic**: Swap magnet/switch or use NC switch
- **False triggers**: Check for electromagnetic interference
