# 📡 IR Transceiver Module  

This repository contains the microcontroller code for the **Centralized ACU Controller** project.  
Asia Pacific College (APC)  
School of Engineering (SoE)  
Academic Year: 2024–2025  

---

## 🔧 Centralized ACU Controller  
> "The agreed requirement to pass capstone."  

A subproject under the **Building Energy Management** initiative, this project aims to develop a centralized dashboard to control and monitor Air Conditioning Units (ACUs) installed in APC classrooms.

---

## Project Details

### Microcontroller
- ESP01M IR Transceiver (ESP8285/ESP8266-based)

### Dashboard
- Bootstrap-based UI

### IoT Components
- MQTT (PubSubClient-based)

---

## Mitsubishi Heavy ACU Protocol

The IR protocol is tailored to the **Mitsubishi Heavy FDE71VNXVG** ACU.  
The transmitter logic is based on a reverse-engineered mobile app that worked with this unit. The official remote (RCN-E-E3) is planned for future integration.

---

## Current Features

- **MQTT-controlled wireless IR transmission**
  - Accepts **JSON-formatted instructions** for:
    - Power (e.g. `"on"`, `"off"`)
    - Mode (e.g. `"cool"`, `"dry"`)
    - Fan speed (e.g. `"low"`, `"auto"`)
    - Temperature (e.g. `24`)
    - Louver position (e.g. `"swing"`, `"3"`)

- **OTA (Over-the-Air) updates via local Wi-Fi**
- **Auto-connect to APC campus Wi-Fi (pre-filled SSID table)**
- **Custom `ACU_remote_encoder` and `ACU_IR_modulator` libraries**  
  *(based on IRremoteESP8266 v2.8.6)*

---

## Usage

The microcontroller receives **IR command instructions via MQTT**, structured as **JSON payloads**. These payloads define the desired ACU state including power, mode, fan speed, temperature, and louver position.

### MQTT Topic

*Replace `<room_id>` with the actual room identifier.*

### JSON Payload Format

```json
{
  "mode": "cool",
  "fanSpeed": 2,
  "temperature": 24,
  "louver": 3,
  "isOn": true
}
```
### Publish via CLI using mosquitto_pub
```
mosquitto_pub -t acu/rooms/room101/set -m '{
  "mode": "cool",
  "fanSpeed": 2,
  "temperature": 24,
  "louver": 3,
  "isOn": true
}'
```

---

## Setup Instructions  
*To be finalized after the refactor of user-initialized variables.*  

To program the microcontroller, you'll need to:
- Install an IDE (preferably VSCode)
- Add PlatformIO extension
- Configure the board (PlatformIO)

To test the remote control, you'll need to:
- Connect to a MQTT Broker
- Hint: Find the `mqtt_server` variable in `MQTT.h` and connect to a corresponding broker and port.
- This could either be a local deployment (via Mosquitto) or a deployed development broker.
- For more context about MQTT: [Introducing the MQTT Protocol | HiveMQ](https://www.hivemq.com/blog/mqtt-essentials-part-1-introducing-mqtt/)
- For mqtt broker configuration: [Refer to Eclipse MQTT Documentation](https://mosquitto.org/documentation/)

To connect via localhost MQTT broker, 
you must `allow_anonymous true` in mosquitto.conf.
Then set the corresponding listener ports.

Default broker configuration:
```
listerner PORT#
protocol mqtt
```

---

## 📅 Timeline and Related Projects

**Course:** _ECEMETH – ECECAP1 – ECECAP2_  
**Academic Year:** Y3T2 to Y4T1

### 🗓️ Development Timeline:
**2024**
- 📦 Mar — [Smart_Home](#smart_home)
- 🌱 Jul — [Aeroponic_SYS](#aeroponic_sys)
- 🌦️ Nov — [ECE221_Weather_Station](#ece221_weather_station)  

**2025**
- 🏠 Mar — [Indoor Air Quality Monitoring](#indoor-air-quality-monitoring)
- ❄️ Apr–Jun — [Centralized ACU Controller](#centralized-acu-controller-on-progress)

**2026**
- 🔜 TBA

---

## 👤 Author

**Keanu Geronimo**  
- GitHub: [@Xrnah](https://github.com/Xrnah)  
- LinkedIn: [Keanu Geronimo](https://www.linkedin.com/in/keanu-geronimo-a77062190/)  
- Portfolio: [Canva - PROFETH Portfolio](https://geronimo-keanu-portfolio.my.canva.site/)

---

## 📜 License

This project is licensed under the MIT License – see the [LICENSE](LICENSE) file for details.
