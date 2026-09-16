---
type: architecture
title: ESP32-S3 Native Matter-over-Wi-Fi Architecture & Endpoints
description: Architecture, partition requirements, and commissioning invariants for native Matter on ESP32-S3.
tags: [matter, esp32s3, smart-home, bme688, m5sticks3]
timestamp: 2026-09-16
---

# ESP32-S3 Native Matter-over-Wi-Fi Architecture

## Overview
This document specifies the architecture and implementation invariants for running native Matter-over-Wi-Fi smart home endpoints on the M5Stack StickS3 (ESP32-S3) with an attached Unit ENV Pro (BME688).

## Key Invariants & Details

### 1. Flash & Partition Scheme
- **Binary Footprint**: The Matter stack (`libCHIP.a`) with Bluetooth Low Energy (BLE) commissioning and Wi-Fi drivers compiles to ~1.95 MB.
- **Partition Requirement**: Standard 4MB partitions with OTA cannot fit the application image. The build requires:
  - `PartitionScheme=huge_app` (3.14MB APP space, no OTA)
  - `FlashSize=8M`
  - `CDCOnBoot=cdc`

### 2. Matter Endpoints & Data Model
The firmware instantiates three standard Matter endpoints registered with the Matter data model:
- **MatterTemperatureSensor (`0x0402`)**: Telemetry reported in 0.01 °C resolution.
- **MatterHumiditySensor (`0x0405`)**: Telemetry reported in 0.01 %RH resolution.
- **MatterPressureSensor (`0x0403`)**: Telemetry reported in 1 hPa units.

### 3. Commissioning & QR Code Invariant
- **Payload Format**: Mobile commissioning apps (Apple Home, Google Home, SmartThings) require the raw alphanumeric Matter onboarding string beginning with `MT:...` (e.g. `MT:Y.K9042C00KA0648G00`). If an HTTP URL is supplied, scanner applications will open a web browser instead of pairing.
- **On-Screen Display**: Rendered via `M5.Display.qrcode(code.c_str(), 6, 23, 96, 1, true)` which provides the mandatory white quiet zone and high-contrast black modules.
- **Manual Pairing Code**: `3497-011-2332` (standard 11-digit pairing code with discriminator and passcode).

### 4. Hardware Power Control
- The AW35122 5V boost converter powering the Grove Port A is disabled on boot.
- The firmware must invoke `M5.Power.setExtOutput(true)` prior to initializing the I2C sensor.

### 5. Factory Reset Procedure
- In accordance with the Matter specification, decommissioning requires clearing the NVS partition:
  - **On-Device**: 10-second Button A hold on the Matter screen triggers `Matter.decommission()` and `ESP.restart()`.
  - **CLI**: `./manage-v2.sh matter-reset` wipes the NVS partition (`0x9000 0x5000`).

## Related Concepts
- [M5Stack StickS3 Hardware Architecture and Sensor Bus Integration](./m5sticks3-uiflow-sensors.md)
