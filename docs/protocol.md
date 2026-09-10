# Hyena BLE Protocol Notes

This document records observations and confirmed decodings of the proprietary Hyena e-bike BLE protocol.

## Telemetry

| Packet / field | Meaning | Scaling | Confidence |
|---|---|---:|---|
| `0203` | Cadence | raw / 40 = RPM | Confirmed experimentally |
| TBD | Speed | TBD | Under investigation |
| TBD | Power | TBD | Under investigation |
| TBD | Battery SoC | TBD | Under investigation |

## Garmin mapping

| Hyena telemetry | Standard BLE service | Status |
|---|---|---|
| Cadence | Cycling Speed & Cadence (CSC) | Proof of concept confirmed |
| Speed | Cycling Speed & Cadence (CSC) | Planned |
| Power | Cycling Power Service (CPS) | Planned |
| Battery SoC | Battery Service | Future investigation |

Keep protocol hypotheses separate from confirmed observations. Update this document when new captures or controlled tests establish a decoding with confidence.
