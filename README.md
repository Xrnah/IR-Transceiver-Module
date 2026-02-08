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
### MQTT Broker
Set broker settings in `include/MQTT.h` (e.g., `mqtt_server`, port, credentials).

### Wi-Fi
Populate the SSID table in the Wi-Fi config header (see `include/` headers used for Wi-Fi setup). This is designed for campus Wi-Fi auto-connect.

## MQTT Usage
### Topic Format
```
control_path/floor_id/room_id/acu_id
```

### JSON Payload Schema
```json
{
  "mode": "cool",
  "fan_speed": 2,
  "temperature": 24,
  "louver": 3,
  "power": true
}
```

### Allowed Values
- `power`: `true | false`
- `mode`: `cool | dry | fan | auto | heat`
- `fan_speed`: `0..3` or `auto`
- `temperature`: `16..30`
- `louver`: `1..5` or `swing`

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
- MQTT not connecting: check broker address/port in `include/MQTT.h`.
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
