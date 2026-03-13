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

Location: `firmware/smart-feeder.cpp`

1. Install Arduino IDE or PlatformIO
2. Install ESP32 board support
3. Update Wi-Fi credentials
4. Upload to ESP32

### Backend API

Location: `backend/`

```bash
cd backend
npm install
npm run dev
```

API runs on http://localhost:3002

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | /health | Health check |
| GET | /api/feeders | List all feeders |
| POST | /api/feeders | Register new feeder |
| POST | /api/status | Update feeder status |
| POST | /api/feed | Trigger feeding |
| GET | /api/schedule | Get schedules |
| POST | /api/schedule | Create schedule |

**Required Header:** `X-API-Key: your-api-key-here`

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

## License

MIT
