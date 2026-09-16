# sticks3-ENV-pro-tool

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Python: 3.10+](https://img.shields.io/badge/Python-3.10%2B-brightgreen.svg)](https://www.python.org/)
[![Hardware: M5StickS3](https://img.shields.io/badge/Hardware-M5Stack%20StickS3-orange.svg)](https://docs.m5stack.com/en/core/StickS3)
[![Sensor: ENV Pro](https://img.shields.io/badge/Sensor-Unit%20ENV%20Pro%20(BME688)-purple.svg)](https://docs.m5stack.com/en/unit/env_pro)

A real-time environmental monitoring dashboard, historical logger, and time-series charting suite for the **M5Stack StickS3** (ESP32-S3) with an attached **Unit ENV Pro** (Bosch BME688) over Grove I2C.

Designed for **MicroPython / UIFlow 2.0**, this tool includes continuous background logging, smart power saving, a 6-view interactive dashboard, and a host-side CLI for live execution and flashing.

---

## UI Preview & Screenshots

| 1. Stick S3 IMU & Spirit Level | 2. ENV Pro Live Overview |
|:---:|:---:|
| ![Stick S3 Sensors](docs/screenshots/01_sticks3_imu.png) | ![ENV Pro Overview](docs/screenshots/02_envpro_overview.png) |
| *BMI270 6-axis IMU, live spirit level crosshair, and battery monitor* | *Live numerical tiles for Temperature, Humidity, Pressure, and Gas* |

| 3. Temperature Historical Chart | 4. Gas Resistance / VOC Chart |
|:---:|:---:|
| ![Temperature Chart](docs/screenshots/03_temp_chart.png) | ![Gas Resistance Chart](docs/screenshots/04_gas_chart.png) |
| *Auto-scaled temperature trend line with live, min, and max readouts* | *Indoor air quality / VOC indicator tracking over time* |

---

## Key Features

- **6 Dedicated Screens (Cycle with Front Button A)**:
  1. **Stick S3 Sensors**: Live BMI270 3-axis Accelerometer, 3-axis Gyroscope, animated spirit level bubble/crosshair, and battery voltage/percentage.
  2. **ENV Pro Live Overview**: 2x2 numerical dashboard showing Temperature (°C), Relative Humidity (%RH), Barometric Pressure (hPa), and Gas Resistance (kΩ).
  3. **Temperature Chart (°C)**: Time-series trend line with live, min, and max indicators.
  4. **Humidity Chart (%RH)**: Real-time relative humidity trend line.
  5. **Barometric Pressure Chart (hPa)**: Atmospheric pressure trend line.
  6. **Gas Resistance Chart (kΩ)**: VOC / air quality fluctuation chart.
- **Continuous 2-Second Background Logging**:
  - Automatically captures readings every **2.0 seconds** into circular ring buffers (up to 120 points = 4 minutes of uninterrupted history).
  - Even when the screen goes to sleep, Grove 5V power remains active and the sensor continues logging in the background.
- **Smart Battery Saving**:
  - **Inactivity Sleep**: Screen backlight turns off after 30 seconds of inactivity to conserve battery while logging continues.
  - **Instant Wakeup**: Pressing either Button A or Button B brings the screen back to full brightness, revealing the complete history collected during sleep.
  - **Manual Sleep Toggle**: Hold Button B for > 1 second to toggle sleep/wake manually.
- **UIFlow 2 App Launcher Integration**:
  - Installed into `/apps/sensor_dashboard.py`, making it directly selectable from UIFlow 2's on-device **"APP LIST"** menu.
  - Gracefully restores portrait orientation (`Display.setRotation(0)`) upon exit so the UIFlow 2 startup menu is never corrupted.

---

## Hardware Requirements & Pinout

| Component | Hardware Details | Connection / Pins |
| :--- | :--- | :--- |
| **Host Device** | M5Stack StickS3 (ESP32-S3-PICO-1-N8R8) | USB-C |
| **Display** | 1.14" IPS LCD (240 × 135 px, ST7789v2) | Internal SPI |
| **Internal IMU** | Bosch BMI270 (6-axis Accelerometer & Gyroscope) | Internal I2C |
| **External Sensor** | M5Stack Unit ENV Pro (Bosch BME688) | Grove Port A (I2C: SDA=GPIO 9, SCL=GPIO 10, Addr=0x77) |
| **Grove Power** | AW35122 5V Boost Converter | Enabled via `M5.Power.setExtOutput(True)` |

---

## Installation & Host Setup

The project uses [uv](https://github.com/astral-sh/uv) (or standard Python `venv`) to manage host communication tools (`mpremote`, `pyserial`, `pillow`).

### 1. Clone the Repository
```bash
git clone https://github.com/alexjmoore/sticks3-ENV-pro-tool.git
cd sticks3-ENV-pro-tool
```

### 2. Set Up Python Environment
```bash
# Using uv (recommended)
uv sync

# Or using standard python3
python3 -m venv .venv
source .venv/bin/activate
pip install -e .
```

### 3. Ensure Serial Permissions (Linux)
Ensure your Linux user has access to serial devices:
```bash
sudo usermod -aG dialout $USER
# Log out and back in, or run for the current session:
sudo chmod 666 /dev/ttyACM0
```

---

## Usage & Management CLI

The included `./manage.sh` script automates device detection, live testing, and flashing:

```bash
# 1. Detect connected StickS3 port
./manage.sh detect

# 2. Query board hardware, MicroPython & UIFlow info
./manage.sh info

# 3. Run the dashboard live in RAM over serial (great for rapid iteration)
./manage.sh run

# 4. Deploy app to device flash (:apps/sensor_dashboard.py and :main.py)
./manage.sh deploy

# 5. Boot Configuration:
./manage.sh set-boot-menu     # Boots into UIFlow 2 startup menu (APP LIST)
./manage.sh set-boot-direct   # Boots directly into the sensor dashboard

# 6. Open interactive MicroPython serial REPL
./manage.sh repl
```

---

## On-Device Controls

| Control | Action | Function |
| :--- | :--- | :--- |
| **Front Button A (M5)** | Short Press | Step through the 6 screens (or wake display) |
| **Front Button A (M5)** | Hold 2.5s | Exit app back to UIFlow 2 startup menu |
| **Side Button B** | Short Press | Flip screen 180° (`Rotation 1` $\leftrightarrow$ `Rotation 3`) |
| **Side Button B** | Long Press (> 1s) | Manually toggle display sleep / wake |

---

## Launching via UIFlow 2 Startup Menu

1. Power on or reset your StickS3 while holding **Button A** (if needed) to enter the startup menu.
2. Use the side button (**Btn B**) to highlight **APP LIST**.
3. Press the front button (**Btn A**) to open the list.
4. Select **`sensor_dashboard.py`** and press **Btn A** to launch!

---

## License

This project is licensed under the **GNU General Public License v3.0** (GPLv3) — see the [LICENSE](LICENSE) file for details.
