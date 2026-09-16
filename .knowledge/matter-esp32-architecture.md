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

### 6. I2C Bus Isolation Invariant
- `M5Unified` on StickS3 attaches its internal driver to hardware controller `I2C1` (GPIO 47/48) for the BMI270 IMU and PMIC.
- External Grove sensors (Unit ENV Pro) MUST be attached to `Wire` (hardware controller `I2C0` on GPIO 9/10).
- Using `Wire1` (`I2C1`) in Arduino code collides directly with `M5.update()`, resulting in continuous communication failure on Grove Port A.

### 7. Matter Endpoint Initialization Sequence Invariant
- Matter endpoint instances (`matterTemp`, `matterHum`, `matterPress`) MUST call `.begin()` **prior to** calling `Matter.begin()`.
- `Matter.begin()` builds the Matter data model and registers endpoint cluster attributes. Calling `endpoint.begin()` after `Matter.begin()` attempts to dereference unlinked attribute tables, triggering a panic (`LoadProhibited` / EXCVADDR `0x00000000`).

### 8. Bluetooth LE Memory Retention Invariant
- By default, `initArduino()` releases Bluetooth controller memory (`esp_bt_controller_mem_release(ESP_BT_MODE_BLE)`) unless `bleInUse()` returns true.
- Because `libMatter` is linked as an archive, the linker does not invoke its constructor before `app_main()`. The main sketch must declare a strong `extern "C" bool bleInUse(void) { return true; }` and include `<esp32-hal-alloc-ble-mem.h>`.
- Without this override, BLE memory is released on boot, causing `Matter.begin()` to silently disable BLE transport, making the device undiscoverable to Google Home and Apple Home during pairing.

### 9. Custom Device Instance Info Provider (Vendor, Product, Hardware Version)
- Endpoint 0 BasicInformation cluster attributes (`VendorName`, `ProductName`, `HardwareVersion`, `HardwareVersionString`, etc.) are immutable attributes in the Matter data model and return `ESP_ERR_INVALID_ARG` (error 262) if modified via `esp_matter::attribute::update()`. Only `NodeLabel` is writable at runtime.
- To provide custom hardware and vendor branding to controllers (Google Home, Apple Home, Alexa, Home Assistant), implement a custom `chip::DeviceLayer::DeviceInstanceInfoProvider` (see `device_info_provider.h`) and register it using `chip::DeviceLayer::SetDeviceInstanceInfoProvider(&provider)`.
- Values provided:
  - **Vendor Name**: `"M5Stack"`
  - **Product Name**: `"StickS3-PRO-Env"`
  - **Hardware Version**: `1` (numeric)
  - **Hardware Version String**: `"v1.0-ESP32S3"`
  - **Part Number**: `"StickS3-BME688"`
  - **Product URL**: `"https://m5stack.com"`
  - **Product Label**: `"M5Stack StickS3 Environmental Monitor"`
  - **Serial Number**: `"M5S3-ENV-2026"`

## Related Concepts
- [M5Stack StickS3 Hardware Architecture and Sensor Bus Integration](./m5sticks3-uiflow-sensors.md)
