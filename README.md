# IR Transceiver Module

ESP01M-based IR transceiver firmware that accepts MQTT JSON commands to control Mitsubishi Heavy AC units.

Status: prototype / lab-tested. IR protocol support is focused on Mitsubishi Heavy FDE71VNXVG.

## Features
- MQTT-controlled wireless IR transmission
- JSON payloads for power, mode, fan speed, temperature, and louver position
- Auto-connect to campus Wi-Fi (pre-filled SSID table)
- Custom `ACU_remote_encoder` and `ACU_IR_modulator` libraries (based on IRremoteESP8266 v2.8.6)
- OTA updates: not enabled (planned)

## Hardware Requirements
- ESP01M IR Transceiver Module (ESP8285/ESP8266-based)
- Widely sold as a prebuilt module by online retailers (e.g., Lazada, Shopee, AliExpress)
- Typical sourcing is from Shenzhen; module branding varies

## Wiring / Pinout
This project assumes the prebuilt ESP01M IR transceiver module. If you are using a bare ESP8285/ESP8266 and discrete IR hardware, you will need to adapt the IR LED driver and receiver wiring accordingly.

## Getting Started
### Prerequisites
- VS Code + PlatformIO
- MQTT broker (local Mosquitto or a dev broker)

### Build & Flash
1. Open the repo in VS Code.
2. Install the PlatformIO extension.
3. Select the target environment in `platformio.ini`.
4. Build and upload:
   ```bash
   pio run
   pio run -t upload
   ```

## Configuration
### Secrets File
Create `include/secrets.h` from the template:
- Copy `include/secrets_template.h` to `include/secrets.h`
- Edit the values in `include/secrets.h`

### Build-Time IR Pipeline Selection
Set `USE_ACU_ADAPTER` in `include/secrets.h`:
- `1` = use IRremoteESP8266 adapters (MHI88/MHI152)
- `0` = use raw IR modulator (based on PJA502A704AA remote)

The reported identity model string is `ACU_REMOTE_MODEL`.

### MQTT Broker Settings
Define broker settings in `include/secrets.h`:
- `MQTT_SERVER`
- `MQTT_PORT`
- `MQTT_USER`
- `MQTT_PASS`

### Topic Roots
Define topic roots in `include/secrets.h`:
- `STATE_PATH` (publish root)
- `CONTROL_PATH` (subscribe root)

### Per-Module Identity
Define module identity in `include/secrets.h`:
- `DEFINED_FLOOR`
- `DEFINED_ROOM`
- `DEFINED_UNIT`
- `DEFINED_ROOM_TYPE_ID`
- `DEFINED_DEPARTMENT`

### Wi-Fi and NTP
Define hidden SSID credentials in `include/secrets.h`:
- `HIDDEN_SSID`
- `HIDDEN_PASS`

Define NTP servers in `include/secrets.h`:
- `NTP_SERVER_1`
- `NTP_SERVER_2`

## MQTT Usage
### Subscribe Topic (Commands)
The device subscribes to:
```
CONTROL_PATH/DEFINED_FLOOR/DEFINED_ROOM/DEFINED_UNIT
```

### Publish Topics (State and Telemetry)
The device publishes to:
```
STATE_PATH/DEFINED_FLOOR/DEFINED_ROOM/DEFINED_UNIT/state
STATE_PATH/DEFINED_FLOOR/DEFINED_ROOM/DEFINED_UNIT/identity
STATE_PATH/DEFINED_FLOOR/DEFINED_ROOM/DEFINED_UNIT/deployment
STATE_PATH/DEFINED_FLOOR/DEFINED_ROOM/DEFINED_UNIT/diagnostics
STATE_PATH/DEFINED_FLOOR/DEFINED_ROOM/DEFINED_UNIT/metrics
```

### JSON Payload Schema
The command handler accepts either a top-level state or a nested `state` object.

Top-level:
```json
{
  "mode": "cool",
  "fan_speed": 2,
  "temperature": 24,
  "louver": 3,
  "power": true
}
```

Nested:
```json
{
  "state": {
    "mode": "cool",
    "fan_speed": 2,
    "temperature": 24,
    "louver": 3,
    "power": true
  }
}
```

### Tight Schema (Validated by Firmware)
Required fields and types (all required):
- `fan_speed`: integer (uint8)
- `temperature`: integer (uint8)
- `mode`: string
- `louver`: integer (uint8)
- `power`: boolean

Accepted values for IR encoding:
- `mode`: `auto | cool | heat | dry | fan`
- `fan_speed`: `1..6`
- `temperature`: `18..30`
- `louver`: `0..4`
- `power`: `true | false`

Notes:
Missing required fields cause the command to be rejected.
Out-of-range values are accepted but encoded to protocol defaults (e.g., unknown temperatures or louver positions map to the encoder defaults).

### Example Publish (mosquitto_pub)
```bash
mosquitto_pub -t control_path/floor_id/room_id/acu_id -m '{
  "mode": "cool",
  "fan_speed": 2,
  "temperature": 24,
  "louver": 3,
  "power": true
}'
```

## Mitsubishi Heavy ACU Protocol
The IR protocol is tailored to Mitsubishi Heavy FDE71VNXVG ACUs.
Transmitter logic was reverse-engineered based on a mobile app (evidently based on the PJA502A704AA remote).
The official remote (RCN-E-E3) is planned for future integration.

## Security / Safety Notes
- For local testing only, you can enable anonymous MQTT by setting `allow_anonymous true` in `mosquitto.conf`.
- Do not expose an anonymous broker to public networks.
- IR requires line-of-sight; placement affects reliability.

## Troubleshooting
- No IR response: verify line-of-sight and module orientation.
- MQTT not connecting: check broker address and port in `include/secrets.h`.
- Device not joining Wi-Fi: confirm SSID table contents and credentials.

## Project Context

This project aims to develop the IR remote module to be deployed on each air conditioning unit (ACU) in various rooms. Accompanying the hardware is a centralized dashboard to control and monitor each transceiver.

Part of the Centralized ACU Project under the Building Energy Management initiative at Asia Pacific College - School of Engineering (APC-SoE), Academic Year 2024–2026.
## Author
Keanu Geronimo  
- GitHub: [@Xrnah](https://github.com/Xrnah)  
- LinkedIn: [Keanu Geronimo](https://www.linkedin.com/in/keanu-geronimo-a77062190/)  
- Portfolio: [Canva - PROFETH Portfolio](https://geronimo-keanu-portfolio.my.canva.site/)  

## License
MIT License. See `LICENSE`.

## Third-Party Notices
See `THIRD_PARTY_NOTICES.md`.

