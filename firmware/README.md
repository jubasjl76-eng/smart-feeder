# Feeder firmware

`main.cpp` on [smart-pet-device-sdk](https://github.com/jubasjl76-eng/smart-pet-device-sdk).
The SDK handles Wi-Fi/SoftAP provisioning, NTP, MQTT, LWT, OTA, command/ack,
schedule caching and the offline journal. This sketch adds: dispense on
`feed` / schedule, `foodLevel` on status, a manual button (GPIO0), a status LED
(GPIO2).

## Build

```bash
pio run -d firmware
```

`platformio.ini` pins the SDK to a commit and forces `-std=gnu++17` (ESP32
Arduino core 2.x defaults lower). Partition scheme is `min_spiffs.csv` so signed
A/B OTA has two ~1.9 MB app slots.

## Wiring (diagram.json — do not change)

| GPIO | part |
|---|---|
| 4  | servo (auger) |
| 5  | HC-SR04 TRIG |
| 18 | HC-SR04 ECHO |
| 2  | status LED |
| 0  | manual-feed button (to 3V3) |

## First boot / provisioning

No credentials are compiled in. On first boot the device opens a Wi-Fi AP
`smartpet-<mac>`; join it and fill in kennelId, deviceId, Wi-Fi, MQTT host and
the claim password. Values persist in NVS.

## Calibrate on a real board

These constants in `main.cpp` need a bench measurement — the simulator can't
give them:

| constant | how to set it |
|---|---|
| `AUGER_GRAMS_PER_SEC` | send one `feed` command, weigh the output, divide grams by the seconds the servo was open (`lastDispenseMs`). Placeholder is 20. |
| `foodLevelPct(fullCm, emptyCm)` in `FeederModule` | measure the HC-SR04 distance with the hopper full and empty; pass those. Defaults 2 cm / 15 cm. |

Also verify on hardware: NTP sync + schedule firing across a reboot, captive
portal on a phone, and an OTA pull end to end.

## CI

- `pio run -d firmware` builds for `esp32dev` on every push (via
  `smart-pet-ci/pio-ci`).
- `firmware/feeder.test.yaml` is a Wokwi smoke test (boot → provisioning portal
  opens). It runs when a `WOKWI_CLI_TOKEN` repo secret is set; free token at
  <https://wokwi.com/dashboard/ci>.
