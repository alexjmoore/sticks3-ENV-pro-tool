#pragma once

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

class BME688Sensor {
public:
  Adafruit_BME680 bme;
  bool connected = false;
  uint8_t i2c_addr = 0x77;

  float temperature = 0.0f;
  float humidity = 0.0f;
  float pressure = 0.0f;
  float gasResistance = 0.0f;

  // Use default Wire (I2C controller 0) to avoid conflict with M5Unified's I2C controller 1
  BME688Sensor() : bme(&Wire) {}

  bool begin() {
    // 1. Configure internal pullups on Grove Port A
    pinMode(9, INPUT_PULLUP);
    pinMode(10, INPUT_PULLUP);

    // 2. Initialize Wire on Grove pins (SDA=9, SCL=10, 100kHz)
    Wire.begin(9, 10, 100000);
    delay(50);

    // 3. Probe I2C addresses (0x77 standard, 0x76 alternate)
    uint8_t found_addr = 0;
    Wire.beginTransmission(0x77);
    if (Wire.endTransmission() == 0) {
      found_addr = 0x77;
    } else {
      Wire.beginTransmission(0x76);
      if (Wire.endTransmission() == 0) {
        found_addr = 0x76;
      }
    }

    if (found_addr == 0) {
      Serial.println("[BME688] I2C probe failed: no ACK at 0x77 or 0x76");
      connected = false;
      return false;
    }

    i2c_addr = found_addr;
    Serial.printf("[BME688] Found sensor at 0x%02X, initializing...\n", i2c_addr);

    // 4. Initialize Adafruit BME680/BME688 driver
    if (!bme.begin(i2c_addr)) {
      Serial.printf("[BME688] bme.begin(0x%02X) failed (chip ID mismatch or communication error)\n", i2c_addr);
      connected = false;
      return false;
    }

    // 5. Configure oversampling, filter, and gas heater
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320°C for 150 ms

    connected = true;
    Serial.printf("[BME688] Initialized successfully on Grove Port A at 0x%02X\n", i2c_addr);
    return true;
  }

  bool read() {
    if (!connected) {
      // Auto-retry connection
      if (!begin()) {
        return false;
      }
    }

    if (!bme.performReading()) {
      Serial.println("[BME688] Failed to perform reading - reconnecting on next cycle");
      connected = false;
      return false;
    }

    temperature = bme.temperature;
    humidity = bme.humidity;
    pressure = bme.pressure / 100.0f;             // Pa to hPa
    gasResistance = bme.gas_resistance / 1000.0f; // Ohms to kOhms
    return true;
  }
};
