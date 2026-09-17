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
    ├── Cycling Power Service (CPS)
    │     └── Rider power
    │
    └── Battery
          └── Battery SoC
    │
    ▼
Garmin watch / cycling computer
```

The project has two deliberately separate parts:

1. **Hyena side** — connect to the e-bike and decode its proprietary BLE telemetry.
2. **Garmin side** — present that telemetry as standard BLE cycling sensors that Garmin devices can consume.

This separation allows the Garmin-facing BLE implementation to be tested with hard-coded values before the Hyena telemetry decoder is integrated.

## Current target

The immediate goal is to make the ESP32-C3 behave as a set of standard BLE sensors with known, hard-coded values. Once Garmin-side behaviour is confirmed, those values will be replaced with live Hyena telemetry.

### Cycling Speed & Cadence (CSC)

The CSC service will provide:

- **Speed** — derived from cumulative wheel revolutions and wheel event time.
- **Cadence** — derived from cumulative crank revolutions and crank event time.

The current development test uses **25 km/h speed** and **90 RPM cadence**. Cadence transmission has already been successfully demonstrated with the Garmin; the current work is adding and validating speed alongside it.

### Cycling Power Service (CPS)

The CPS service will provide **rider power in watts** to the Garmin.

The next stage is to validate this independently using a hard-coded power value before connecting it to Hyena power telemetry.

### Battery / State of Charge

Battery state of charge (SoC) is the next Garmin-facing sensor to investigate.

The preferred first approach is the standard Bluetooth SIG **Battery Service (0x180F)** with **Battery Level (0x2A19)**. This is much simpler than emulating a proprietary e-bike protocol and will be used if it provides the required Garmin functionality.

If the Garmin does not expose the standard Battery Service in the desired way, the project may instead investigate emulating the relevant **Shimano STEPS BLE** services used by Garmin-compatible Shimano e-bikes.

## Hyena telemetry

The Hyena protocol reverse-engineering work has identified several useful telemetry fields:

| Packet | Interpretation | Status |
|---|---|---|
| `0203` | Pedal cadence signal; raw value / 40 = RPM | Confirmed |
| `0207` | Wheel rotational-speed signal | High confidence |
| `0402` | Battery SoC (%) | Confirmed |
| `0401` | Battery voltage (mV) and signed battery current (mA) | High confidence |
| `0202` | Lifetime odometer (m) | Confirmed |

The ESP32 firmware will eventually translate these proprietary values into the standard Garmin-facing services above.

## Repository structure

```text
hyena-garmin-bridge/
├── hyena-garmin-bridge.ino   # Main application
├── HyenaBike.h/.cpp           # Hyena BLE connection and telemetry decoding
├── GarminCSC.h/.cpp           # Bluetooth Cycling Speed & Cadence service
└── GarminCPS.h/.cpp           # Bluetooth Cycling Power Service

docs/
└── protocol.md                # Reverse-engineering notes and telemetry mapping
```

## Development status

- [x] ESP32-C3 Arduino project skeleton
- [x] Garmin CSC cadence proof of concept — Garmin successfully reports cadence
- [x] Hyena cadence decoding identified (`0203`, raw value / 40 = RPM)
- [ ] Validate CSC speed with hard-coded value
- [ ] Validate CPS rider power with hard-coded value
- [ ] Validate standard Bluetooth Battery Service / SoC with Garmin
- [ ] Integrate Hyena BLE connection and telemetry notifications
- [ ] Replace hard-coded CSC values with Hyena speed and cadence telemetry
- [ ] Replace hard-coded CPS value with Hyena rider power telemetry
- [ ] Replace hard-coded battery value with Hyena battery SoC
- [ ] Investigate Shimano STEPS emulation only if required for Garmin battery/e-bike functionality

## Hardware

- ESP32-C3
- Hyena-compatible e-bike
- Garmin device supporting BLE cycling sensors

## Development

Use the Arduino IDE with an ESP32-C3 board package. Required BLE functionality should use the ESP32 Arduino BLE APIs/libraries compatible with the selected ESP32 core version.

## Disclaimer

This is an independent reverse-engineering project and is not affiliated with Hyena, Garmin, Shimano, or their respective manufacturers.
