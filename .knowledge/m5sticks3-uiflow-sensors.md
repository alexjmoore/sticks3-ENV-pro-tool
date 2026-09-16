---
type: architecture
title: M5Stack StickS3 Hardware Architecture and Sensor Bus Integration
description: Hardware pinouts, Grove power control, and I2C bus isolation guidelines for M5Stack StickS3 with external Grove sensors in MicroPython/UIFlow2.
tags: [m5stack, sticks3, esp32s3, uiflow2, micropython, sensors, bme688, i2c]
timestamp: 2026-09-16
---

# M5Stack StickS3 Hardware Architecture and Sensor Bus Integration

## Overview
The **M5Stack StickS3** is an ESP32-S3-PICO-1-based development board featuring an internal 6-axis IMU (BMI270), PMIC battery monitor, 1.14" IPS LCD (ST7789v2, 240x135), and a custom Grove expansion port.

## Key Hardware Invariants & Details

### 1. Grove Port Power Management
- External Grove 5V rail is unpowered by default to conserve battery.
- It is powered by an AW35122 boost converter controlled by GPIO 4.
- In UIFlow2 / M5Unified, it MUST be enabled via:
  ```python
  M5.Power.setExtOutput(True)
  ```

### 2. Grove Pinout & I2C Bus Isolation
- **Grove SDA**: GPIO 9
- **Grove SCL**: GPIO 10
- **Internal I2C**: Used internally by M5 for the BMI270 IMU (address `0x68`) and PMIC.
- **Hardware Contention**: Creating a hardware `machine.I2C(1, ...)` or `machine.I2C(0, ...)` on pins 9 and 10 can conflict with M5's internal I2C driver when calling `M5.update()` and `M5.Imu.getAccel()`, resulting in `[Errno 19] ENODEV`.
- **Solution**: Use software I2C (`SoftI2C`) for the Grove port with internal pull-ups:
  ```python
  from machine import Pin, SoftI2C
  grove_i2c = SoftI2C(
      sda=Pin(9, Pin.IN, Pin.PULL_UP),
      scl=Pin(10, Pin.IN, Pin.PULL_UP),
      freq=100000
  )
  ```

### 3. ENV Pro Unit (Bosch BME688)
- I2C Address: `0x77` (or `0x76`).
- Driver: UIFlow2 provides `unit.ENVPROUnit(grove_i2c)`.
- Capabilities: `get_temperature()`, `get_humidity()`, `get_pressure()`, `get_gas_resistance()`.

### 4. Display Rotation Invariant & UIFlow 2 Menu
- Physical LCD is portrait: 135 (W) × 240 (H).
- The hardware ST7789 display controller retains its rotation setting across soft reboots.
- The UIFlow 2 default startup menu (`startup()`) assumes portrait orientation (`rotation = 0`). If an app leaves the display in landscape (`rotation = 1`), the UIFlow menu draws into 240x135, chopping off the bottom of the UI.
- Fix:
  1. Apps changing rotation must restore `Display.setRotation(0)` inside a `finally` block or when exiting.
  2. In `boot.py`, call `M5.Display.setRotation(0)` before calling `startup(boot_option, ...)`.

## Related Concepts
- [Index](./index.md)

