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

## 📚 Project Details

### 🧠 Microcontroller
- **ESP01M IR Transceiver** (ESP8285/ESP8266-based)

### 🖥️ Dashboard
- **Grafana Enterprise**

### 🗄️ Database
- **MySQL / MariaDB**

### 🧰 Database Manager
- **phpMyAdmin**

### 🌐 IoT Components
- **MQTT (PubSubClient-based)**
- **PHP-based REST API** *(in progress)*

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

## 🧊 Mitsubishi Heavy ACU Protocol

The IR protocol is tailored to the **Mitsubishi Heavy FDE71VNXVG** ACU.  
The transmitter logic is based on a reverse-engineered mobile app that worked with this unit. The official remote (RCN-E-E3) is planned for future integration.

---

## 🚀 Current Features

- 🛜 **MQTT-controlled wireless IR transmission**
- ♻️ **OTA (Over-the-Air) updates via local Wi-Fi**
- 📶 **Auto-connect to APC campus Wi-Fi (pre-filled SSID table)**
- 🛠️ **Custom `ACU_remote_encoder` and `ACU_IR_modulator` libraries**  
  *(based on IRremoteESP8266 v2.8.6)*

---

## ⚙️ Setup Instructions  
*To be added after refactor of user-initialized variables.*  
Setup will include:
- PlatformIO and/or Arduino IDE installation
- Board configuration for ESP01M
- OTA deployment guide
- MQTT server setup
- PHP dashboard API integration

---

## 👤 Author

**Keanu Geronimo**  
- GitHub: [@Xrnah](https://github.com/Xrnah)  
- LinkedIn: [Keanu Geronimo](https://www.linkedin.com/in/keanu-geronimo-a77062190/)  
- Portfolio: [Canva - PROFETH Portfolio](https://geronimo-keanu-portfolio.my.canva.site/)

---

## 📜 License

This project is licensed under the MIT License – see the [LICENSE](LICENSE) file for details.
