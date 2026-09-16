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
#include <esp_wifi.h>
#include <M5Unified.h>

#include <Matter.h>
#include <esp_matter.h>
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
#include "device_info_provider.h"

// Matter Endpoints
MatterTemperatureSensor matterTemp;
MatterHumiditySensor matterHum;
MatterPressureSensor matterPress;

// Custom Device Info Provider
static m5stack::StickS3DeviceInfoProvider deviceInfoProvider;

// Hardware & UI instances
BME688Sensor bme;
DisplayUI ui;

// Timers & Intervals for Power Efficiency
unsigned long lastSensorPoll = 0;
unsigned long lastActivityTime = 0;
unsigned long lastImuPoll = 0;
unsigned long lastGasPoll = 0;

const unsigned long SENSOR_INTERVAL_ACTIVE = 5000;   // 5s when display is ON (smooth UI)
const unsigned long SENSOR_INTERVAL_SLEEP  = 30000;  // 30s when display is OFF (battery saver)
const unsigned long GAS_INTERVAL           = 15000;  // 15s gas heater interval when display is ON
const unsigned long IMU_INTERVAL           = 100;    // 100ms fast IMU polling for auto-orientation
const unsigned long AUTO_SLEEP_TIMEOUT     = 30000;  // 30s inactivity auto-sleep

// Reset hold timer (Button B on Screen 7)
unsigned long btnBHoldStart = 0;
bool isHoldingReset = false;

void setup() {
  // Downclock ESP32-S3 from 240 MHz to 160 MHz for power efficiency and cool operation
  setCpuFrequencyMhz(160);

  Serial.begin(115200);
  delay(300);

  Serial.println("\n==========================================");
  Serial.println("  M5StickS3 + ENV Pro Matter Firmware v2.0");
  Serial.printf ("  CPU Clock: %d MHz (Low-Power Optimized)\n", getCpuFrequencyMhz());
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

  // Set Custom Device Instance Info Provider (Vendor, Product, Hardware Version)
  deviceInfoProvider.init();

  // 6. Initialize Matter Core Stack (Last step after all endpoints are registered)
  Serial.println("[Matter] Initializing Matter runtime...");
  Matter.begin();

  // Ensure custom provider is active after stack start
  deviceInfoProvider.init();

  // Enable Wi-Fi modem sleep (DTIM beacon sleep) for drastic power & heat reduction
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

  // Set friendly Node Label in Basic Information Cluster (displayed by Google Home / Apple Home)
  esp_matter_attr_val_t nameVal = esp_matter_char_str((char*)"StickS3-PRO-Env", strlen("StickS3-PRO-Env"));
  esp_matter::attribute::update(0, chip::app::Clusters::BasicInformation::Id, chip::app::Clusters::BasicInformation::Attributes::NodeLabel::Id, &nameVal);

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
        Serial.println("[Matter] Wi-Fi Connectivity Changed. Re-asserting modem sleep.");
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
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
  lastSensorPoll = millis() - SENSOR_INTERVAL_ACTIVE;
  lastImuPoll = millis();
  lastGasPoll = millis();
  ui.needsFullRedraw = true;
}

void loop() {
  M5.update();
  unsigned long now = millis();

  // --- Fast IMU Polling (100ms) & Accelerometer Auto-Orientation ---
  if (now - lastImuPoll >= IMU_INTERVAL) {
    lastImuPoll = now;
    M5.Imu.getAccel(&ui.ax, &ui.ay, &ui.az);
    M5.Imu.getGyro(&ui.gx, &ui.gy, &ui.gz);

    // Auto-rotate between normal landscape (1) and inverted landscape (3)
    // Deadband hysteresis: only rotate when tilted distinctly (|az| < 0.85g)
    if (fabsf(ui.az) < 0.85f) {
      if (ui.ax > 0.35f && ui.rotation != 1) {
        ui.rotation = 1;
        M5.Display.setRotation(1);
        ui.needsFullRedraw = true;
      } else if (ui.ax < -0.35f && ui.rotation != 3) {
        ui.rotation = 3;
        M5.Display.setRotation(3);
        ui.needsFullRedraw = true;
      }
    }
  }

  // --- Button Handling ---
  bool btnAPressed   = M5.BtnA.wasPressed();
  bool btnBPressed   = M5.BtnB.wasPressed();
  bool btnBIsPressed = M5.BtnB.isPressed();

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
    // Btn A: Step through 7 views (only if not holding reset)
    else if (btnAPressed && !isHoldingReset) {
      ui.stepView();
      lastActivityTime = now;
    }
    // Btn B on Views 0-5: Manual toggle display sleep
    else if (btnBPressed && ui.currentView != VIEW_MATTER) {
      ui.sleepDisplay();
      lastActivityTime = now;
    }
  }

  // --- Matter Factory Reset on View 6 (Hold Button B for 4s) ---
  if (ui.currentView == VIEW_MATTER && ui.displayOn) {
    if (btnBIsPressed) {
      lastActivityTime = now; // Keep display awake while holding
      if (btnBHoldStart == 0) {
        btnBHoldStart = now;
      }
      unsigned long elapsed = now - btnBHoldStart;
      if (elapsed >= 1000) { // After 1s, show 3s countdown
        isHoldingReset = true;
        int remaining = 4 - (int)(elapsed / 1000);
        if (remaining <= 0) {
          M5.Display.fillScreen(TFT_RED);
          M5.Display.setTextColor(TFT_WHITE, TFT_RED);
          M5.Display.setTextSize(2);
          M5.Display.drawString("RESETTING...", 35, 40);
          M5.Display.setTextSize(1);
          M5.Display.drawString("Erasing Fabric & Restarting", 30, 75);
          Serial.println("[Matter] Factory Reset triggered by Button B hold. Decommissioning...");
          Matter.decommission();
          delay(1000);
          ESP.restart();
        } else {
          ui.drawResetCountdown(remaining);
        }
      }
    } else {
      if (btnBHoldStart != 0) {
        btnBHoldStart = 0;
        if (isHoldingReset) {
          isHoldingReset = false;
          ui.needsFullRedraw = true;
        }
      }
    }
  }

  // --- Adaptive Background Sensor & Matter Polling (5s active / 30s sleep) ---
  unsigned long currentInterval = ui.displayOn ? SENSOR_INTERVAL_ACTIVE : SENSOR_INTERVAL_SLEEP;
  if (now - lastSensorPoll >= currentInterval) {
    lastSensorPoll = now;

    // 1. Read Power / Battery
    ui.batLevel = M5.Power.getBatteryLevel();
    ui.vbat = M5.Power.getBatteryVoltage();
    ui.isCharging = M5.Power.isCharging();

    // 2. Decide if gas sensor heater should run (only when display is ON and every 15s)
    bool shouldReadGas = ui.displayOn && ((now - lastGasPoll) >= GAS_INTERVAL);
    if (shouldReadGas) {
      lastGasPoll = now;
    }

    // 3. Read BME688
    if (bme.read(shouldReadGas)) {
      ui.temp_c = bme.temperature;
      ui.humidity = bme.humidity;
      ui.pressure = bme.pressure;
      if (shouldReadGas) {
        ui.gas_res = bme.gasResistance;
      }

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
