# Hyena BLE Translator

Translate the proprietary Hyena e-bike BLE protocol to standard Bluetooth cycling services for use with Garmin devices.

The target hardware is an **ESP32-C3** programmed using the Arduino framework.

## Architecture

```text
Hyena e-bike
    │
    │ proprietary BLE
    ▼
ESP32-C3
    │
    ├── Cycling Speed & Cadence (CSC)
    │     ├── Speed
    │     └── Cadence
    │
    └── Cycling Power Service (CPS)
          └── Power
    │
    ▼
Garmin watch / cycling computer
```

The Hyena protocol decoder is deliberately separated from the Garmin BLE implementation. This allows proprietary telemetry decoding to be developed independently from the standard BLE services presented to Garmin.

## Current status

- [x] ESP32-C3 Arduino project skeleton
- [x] Garmin cadence proof of concept — Garmin successfully reports cadence
- [x] Hyena cadence decoding identified (`0203`, raw value / 40 = RPM)
- [ ] Integrate Hyena BLE connection and telemetry notifications
- [ ] Integrate confirmed cadence decoder
- [ ] Decode and expose speed via CSC
- [ ] Identify useful power telemetry
- [ ] Expose power via CPS
- [ ] Investigate Battery Service / SoC

## Repository structure

```text
src/
├── hyena-garmin-bridge.ino   # Main application
├── HyenaBike.h/.cpp           # Hyena BLE connection and telemetry decoding
├── GarminCSC.h/.cpp           # Standard Cycling Speed & Cadence service
└── GarminCPS.h/.cpp           # Standard Cycling Power Service

docs/
└── protocol.md                # Reverse-engineering notes and telemetry mapping
```

## Hardware

- ESP32-C3
- Hyena-compatible e-bike
- Garmin device supporting BLE cycling sensors

## Development

Use the Arduino IDE with an ESP32-C3 board package. Required BLE functionality should use the ESP32 Arduino BLE APIs/libraries compatible with the selected ESP32 core version.

## Disclaimer

This is an independent reverse-engineering project and is not affiliated with Hyena, Garmin, or their respective manufacturers.
