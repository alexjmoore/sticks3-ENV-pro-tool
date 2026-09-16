#pragma once

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

class BME688Sensor {
public:
  Adafruit_BME680 bme;
  bool connected = false;

  float temperature = 0.0f;
  float humidity = 0.0f;
  float pressure = 0.0f;
  float gasResistance = 0.0f;

  BME688Sensor() : bme(&Wire1) {}

  bool begin() {
    // Grove I2C on StickS3: SDA=GPIO 9, SCL=GPIO 10
    Wire1.begin(9, 10, 100000);

    if (bme.begin(0x77)) {
      connected = true;
    } else if (bme.begin(0x76)) {
      connected = true;
    } else {
      connected = false;
      Serial.println("[BME688] Sensor not found on Grove port (0x77/0x76)");
      return false;
    }

    // Set up oversampling and filter
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320°C for 150 ms


    Serial.println("[BME688] Initialized successfully on Grove Port A");
    return true;
  }

  bool read() {
    if (!connected) return false;

    if (!bme.performReading()) {
      Serial.println("[BME688] Failed to perform reading");
      return false;
    }

    temperature = bme.temperature;
    humidity = bme.humidity;
    pressure = bme.pressure / 100.0f; // Pa to hPa
    gasResistance = bme.gas_resistance / 1000.0f; // Ohms to kOhms
    return true;
  }
};
