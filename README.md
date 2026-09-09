# Smart Pet Feeder

A Wi-Fi enabled smart pet feeder with ESP32, scheduled feeding, and REST API control.

## Features

- 🌐 **Wi-Fi Connected** - Control from anywhere via API
- ⏰ **Scheduled Feeding** - Set multiple feeding times
- 📊 **Food Level Monitoring** - Ultrasonic sensor for level tracking
- 🔒 **API Authentication** - Secure API key based access
- 📝 **Event Logging** - Track all feeding events
- 🔧 **Modular Design** - Basic and advanced modes

## Hardware

### Components

| Component | Model | Cost (€) |
|-----------|-------|----------|
| Microcontroller | ESP32 DevKit V1 | 10 |
| Servo Motor | SG90 | 3 |
| Ultrasonic Sensor | HC-SR04 | 3 |
| Power Supply | 5V 2A | 6 |
| Misc (wires, resistors) | - | 5 |

**Total: ~€27**

### Pin Configuration

| Pin | Component |
|-----|-----------|
| 4 | Servo Signal |
| 5 | Ultrasonic Trig |
| 18 | Ultrasonic Echo |
| 2 | LED Indicator |

## Software

### Firmware

Location: `firmware/main.cpp` — built on
[smart-pet-device-sdk](https://github.com/jubasjl76-eng/smart-pet-device-sdk).
The SDK owns Wi-Fi + SoftAP provisioning, NTP, MQTT
(`kennel/{kennelId}/feeder/{deviceId}/*`), LWT, OTA, command/ack, schedule
caching and the offline journal; `main.cpp` is just the feeder behaviour. See
`firmware/README.md` for build / flash / calibration.

```bash
pio run -d firmware
```

The pre-SDK single-file firmware is kept as `firmware/smart-feeder.legacy.cpp`.

### Backend

There is no per-device backend any more. The feeder talks MQTT to
**[smart-pet-backend](https://github.com/jubasjl76-eng/smart-pet-backend)**,
which owns feeders, schedules, status and the console. The legacy `backend/`
folder in this repo is dead and will be removed.

## 3D Design

Location: `3d-design/feeder-enclosure.scad`

Open in OpenSCAD to view and export STL files.

### Parts
- Main enclosure
- Removable lid
- Motor mount
- Sensor mount
- ESP32 mount

## Getting Started

1. Order components (~€27)
2. 3D print enclosure
3. Assemble hardware
4. Flash firmware
5. Start backend API
6. Register feeder
7. Set feeding schedule

## Wokwi Simulation

This firmware can be simulated in Wokwi without physical hardware.

### Simulated Hardware Components

- **ESP32 DevKit V1** - Main microcontroller
- **Servo Motor** - Food dispensing mechanism
- **Ultrasonic Sensor** - Food level monitoring
- **LED** - Status indicator (green)
- **Push Button** - Manual feed trigger

### Running the Simulation

```bash
pio run -d firmware
WOKWI_CLI_TOKEN=<token> wokwi-cli firmware --scenario firmware/feeder.test.yaml --timeout 20000
```

`firmware/feeder.test.yaml` is the CI smoke test: it boots the emulated ESP32
and asserts the SDK opens the provisioning portal (no NVS creds). CI runs it on
every push when a `WOKWI_CLI_TOKEN` repo secret is set. Get a free token at
<https://wokwi.com/dashboard/ci>.

Or open `firmware/diagram.json` at <https://wokwi.com> and drop in the built
`firmware/.pio/build/esp32dev/firmware.bin`.

### Pin Connections

| ESP32 Pin | Component |
|-----------|-----------|
| 4 | Servo PWM |
| 5 | Ultrasonic Trig |
| 18 | Ultrasonic Echo |
| 2 | Status LED |
| 0 | Push Button |

### Testing

The simulation will show:
- Servo rotating when feeding is triggered
- Ultrasonic sensor measuring food level
- LED blinking on status changes
- Button press triggering feed events

## License

MIT
