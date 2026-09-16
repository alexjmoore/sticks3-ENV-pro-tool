/*
 * M5Stack StickS3 + ENV Pro (BME688) Matter-over-Wi-Fi Smart Home Firmware
 * ------------------------------------------------------------------------
 * Exposes:
 * - Matter Temperature Sensor Endpoint (0x0402)
 * - Matter Relative Humidity Sensor Endpoint (0x0405)
 * - Matter Pressure Sensor Endpoint (0x0403)
 *
 * Provides:
 * - On-device 7-screen interactive UI with M5GFX (IMU spirit level, 4 charts, Matter QR code)
 * - BLE Commissioning for Apple Home, Google Home, Alexa, and Home Assistant
 * - Dual Factory Reset: 10s Button A hold on Matter screen or CLI command
 */

#include <Arduino.h>
#include <WiFi.h>
#include <M5Unified.h>

#include <Matter.h>
#include <MatterEndpoints/MatterTemperatureSensor.h>
#include <MatterEndpoints/MatterHumiditySensor.h>
#include <MatterEndpoints/MatterPressureSensor.h>

// CRITICAL: Prevent Arduino core from releasing Bluetooth LE memory on boot
#include <esp32-hal-alloc-ble-mem.h>
#include <esp32-hal-bt.h>

extern "C" bool bleInUse(void) {
  return true;
}

#include "bme688_sensor.h"
#include "display_ui.h"

// Matter Endpoints
MatterTemperatureSensor matterTemp;
MatterHumiditySensor matterHum;
MatterPressureSensor matterPress;

// Hardware & UI instances
BME688Sensor bme;
DisplayUI ui;

// Timers
unsigned long lastSensorPoll = 0;
unsigned long lastActivityTime = 0;
const unsigned long SENSOR_INTERVAL = 2000;  // 2 seconds
const unsigned long AUTO_SLEEP_TIMEOUT = 30000; // 30 seconds

// Reset hold timer
unsigned long btnAHoldStart = 0;
bool isHoldingReset = false;

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n==========================================");
  Serial.println("  M5StickS3 + ENV Pro Matter Firmware v2.0");
  Serial.println("==========================================\n");

  // 1. Initialize M5 hardware
  auto cfg = M5.config();
  M5.begin(cfg);

  // 2. Power on Grove 5V boost converter (AW35122 on GPIO 4 & M5pm1)
  M5.Power.setExtOutput(true);
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  delay(300);

  // 3. Initialize Display UI
  ui.init();
  ui.drawHeader("STARTING MATTER...", TFT_CYAN);
  M5.Display.drawString("Initializing Matter stack...", 20, 50);

  // 4. Initialize BME688 sensor on Grove port
  bme.begin();

  // 5. Register Matter Endpoints (MUST be before Matter.begin())
  matterTemp.begin(25.0);
  matterHum.begin(50.0);
  matterPress.begin(1013.25);

  // 6. Initialize Matter Core Stack (Last step after all endpoints are registered)
  Serial.println("[Matter] Initializing Matter runtime...");
  Matter.begin();

  // Set initial QR code and manual pairing payload for UI
  ui.qrPayload = Matter.getOnboardingQRCodeUrl();
  ui.manualCode = Matter.getManualPairingCode();

  Serial.printf("[Matter] Manual Pairing Code: %s\n", ui.manualCode.c_str());
  Serial.printf("[Matter] Onboarding QR URL: %s\n", ui.qrPayload.c_str());
  Serial.printf("[Matter] BLE Commissioning Enabled: %s\n", Matter.isBLECommissioningEnabled() ? "YES" : "NO");
  Serial.printf("[Matter] BLE Memory Released: %s\n", btMemReleased(BT_MODE_BLE) ? "YES (ERROR!)" : "NO (OK)");
  Serial.printf("[Matter] Device Commissioned: %s\n", Matter.isDeviceCommissioned() ? "YES" : "NO (Advertising on BLE)");

  // Matter Event Callback
  Matter.onEvent([](matterEvent_t event, const chip::DeviceLayer::ChipDeviceEvent *deviceEvent) {
    switch (event) {
      case MATTER_COMMISSIONING_COMPLETE:
        Serial.println("[Matter] Commissioning Complete! Joined Matter fabric.");
        ui.matterCommissioned = true;
        ui.needsFullRedraw = true;
        break;
      case MATTER_WIFI_CONNECTIVITY_CHANGE:
        Serial.println("[Matter] Wi-Fi Connectivity Changed.");
        break;
      case MATTER_INTERFACE_IP_ADDRESS_CHANGED:
        Serial.printf("[Matter] IP Address: %s\n", WiFi.localIP().toString().c_str());
        ui.ipAddr = WiFi.localIP().toString();
        ui.needsFullRedraw = true;
        break;
      default:
        break;
    }
  });

  lastActivityTime = millis();
  lastSensorPoll = millis() - SENSOR_INTERVAL;
  ui.needsFullRedraw = true;
}

void loop() {
  M5.update();
  unsigned long now = millis();

  // --- Button Handling ---
  bool btnAPressed = M5.BtnA.wasPressed();
  bool btnBPressed = M5.BtnB.wasPressed();
  bool btnBHold    = M5.BtnB.wasHold();

  // Handle Display Sleep / Wakeup
  if (!ui.displayOn) {
    if (btnAPressed || btnBPressed) {
      ui.wakeDisplay();
      lastActivityTime = now;
      delay(50);
    }
  } else {
    // Check Auto-Sleep (30s inactivity)
    if (now - lastActivityTime >= AUTO_SLEEP_TIMEOUT) {
      ui.sleepDisplay();
    }
    // Manual sleep toggle on Btn B hold
    else if (btnBHold) {
      ui.sleepDisplay();
      lastActivityTime = now;
    }
    // Btn A: Step through 7 views
    else if (btnAPressed && !isHoldingReset) {
      ui.stepView();
      lastActivityTime = now;
    }
    // Btn B: Flip 180° orientation
    else if (btnBPressed) {
      ui.toggleRotation();
      lastActivityTime = now;
    }
  }

  // --- Matter Factory Reset on View 6 (Hold Btn A for 10s) ---
  if (ui.currentView == VIEW_MATTER && ui.displayOn) {
    if (M5.BtnA.isPressed()) {
      if (btnAHoldStart == 0) {
        btnAHoldStart = now;
      }
      unsigned long elapsed = now - btnAHoldStart;
      if (elapsed >= 7000) { // Last 3 seconds show countdown
        isHoldingReset = true;
        int remaining = 10 - (int)(elapsed / 1000);
        if (remaining <= 0) {
          M5.Display.fillScreen(TFT_RED);
          M5.Display.setTextColor(TFT_WHITE, TFT_RED);
          M5.Display.drawString("RESETTING MATTER...", 30, 50);
          Serial.println("[Matter] Factory Reset triggered by user. Decommissioning...");
          Matter.decommission();
          delay(1000);
          ESP.restart();
        } else {
          ui.drawResetCountdown(remaining);
        }
      }
    } else {
      if (btnAHoldStart != 0) {
        btnAHoldStart = 0;
        if (isHoldingReset) {
          isHoldingReset = false;
          ui.needsFullRedraw = true;
        }
      }
    }
  }

  // --- 2-Second Background Sensor & Matter Polling (ALWAYS RUNS) ---
  if (now - lastSensorPoll >= SENSOR_INTERVAL) {
    lastSensorPoll = now;

    // 1. Read IMU
    M5.Imu.getAccel(&ui.ax, &ui.ay, &ui.az);
    M5.Imu.getGyro(&ui.gx, &ui.gy, &ui.gz);

    // 2. Read Power / Battery
    ui.batLevel = M5.Power.getBatteryLevel();
    ui.vbat = M5.Power.getBatteryVoltage();
    ui.isCharging = M5.Power.isCharging();

    // 3. Read BME688
    if (bme.read()) {
      ui.temp_c = bme.temperature;
      ui.humidity = bme.humidity;
      ui.pressure = bme.pressure;
      ui.gas_res = bme.gasResistance;

      // Append to on-screen historical chart buffers
      ui.appendHistory(ui.temp_c, ui.humidity, ui.pressure, ui.gas_res);

      // 4. Update Matter Endpoints
      matterTemp.setTemperature(ui.temp_c);
      matterHum.setHumidity(ui.humidity);
      matterPress.setPressure(ui.pressure);
    }

    // Update Matter & Wi-Fi status
    ui.matterCommissioned = Matter.isDeviceCommissioned();
    ui.matterConnected = Matter.isDeviceConnected();
    if (WiFi.status() == WL_CONNECTED) {
      ui.ipAddr = WiFi.localIP().toString();
      ui.wifiSSID = WiFi.SSID();
    }

    // Refresh display if screen is ON and not in reset countdown
    if (ui.displayOn && !isHoldingReset) {
      ui.updateDisplay();
    }
  }

  delay(20);
}
