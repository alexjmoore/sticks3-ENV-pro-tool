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
#include <nvs_flash.h>
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
const unsigned long SENSOR_INTERVAL_SLEEP  = 60000;  // 60s when display is OFF (battery saver)
const unsigned long GAS_INTERVAL           = 15000;  // 15s gas heater interval when display is ON
const unsigned long IMU_INTERVAL           = 100;    // 100ms fast IMU polling for auto-orientation
const unsigned long AUTO_SLEEP_TIMEOUT     = 15000;  // 15s fast auto-sleep
const unsigned long DOUBLE_CLICK_TIME      = 280;    // 280ms window for double tap detection

// Delta-threshold tracking to eliminate redundant Matter RF packets
static float lastReportedTemp = -999.0f;
static float lastReportedHum = -999.0f;
static float lastReportedPress = -999.0f;

// Reset hold timer (Button B on Screen 8)
unsigned long btnBHoldStart = 0;
bool isHoldingReset = false;
int lastReportedRemaining = -1;

// Double-click navigation timer (Button A)
unsigned long lastBtnAPressTime = 0;
bool pendingBtnASingleClick = false;

void setup() {
  // Downclock ESP32-S3 from 240 MHz to 80 MHz for power efficiency and cool operation
  setCpuFrequencyMhz(80);

  Serial.begin(115200);
  delay(300);

  Serial.println("\n==========================================");
  Serial.println("  M5StickS3 + ENV Pro Matter Firmware v2.0");
  Serial.printf ("  CPU Clock: %d MHz (Ultra Low-Power & Cool)\n", getCpuFrequencyMhz());
  Serial.println("==========================================\n");

  // Resilient NVS initialization: auto-recover if partition is corrupted or full
  esp_err_t nvs_err = nvs_flash_init();
  if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    Serial.println("[NVS] Fragmented/full partition detected. Re-formatting NVS...");
    nvs_flash_erase();
    nvs_flash_init();
  }

  // 1. Initialize M5 hardware with unused high-power peripherals disabled
  auto cfg = M5.config();
  cfg.internal_spk = false; // Disable ES8311 audio DAC & speaker amplifier (eliminates continuous idle heat)
  cfg.internal_mic = false; // Disable microphone I2S peripheral
  cfg.output_power = true;  // Power Grove port 5V via M5PM1 PMIC
  M5.begin(cfg);

  // Ensure audio speaker/mic peripherals and analog amplifiers are dormant
  M5.Speaker.end();
  M5.Mic.end();

  // 2. Power on Grove 5V boost converter via M5PM1 PMIC
  M5.Power.setExtOutput(true);
  delay(100);

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

  // Set initial QR code and manual pairing payload for UI
  ui.qrPayload = Matter.getOnboardingQRCodeUrl();
  ui.manualCode = Matter.getManualPairingCode();

  Serial.printf("[Matter] Manual Pairing Code: %s\n", ui.manualCode.c_str());
  Serial.printf("[Matter] Onboarding QR URL: %s\n", ui.qrPayload.c_str());
  Serial.printf("[Matter] BLE Commissioning Enabled: %s\n", Matter.isBLECommissioningEnabled() ? "YES" : "NO");
  Serial.printf("[Matter] BLE Memory Released: %s\n", btMemReleased(BT_MODE_BLE) ? "YES (ERROR!)" : "NO (OK)");
  Serial.printf("[Matter] Device Commissioned: %s\n", Matter.isDeviceCommissioned() ? "YES" : "NO (Advertising on BLE)");

  // If already commissioned, disable BLE advertising in the Matter stack to save battery
  if (Matter.isDeviceCommissioned()) {
    Serial.println("[Power] Device is already commissioned. Disabling BLE advertising to save battery.");
    chip::DeviceLayer::ConnectivityMgr().SetBLEAdvertisingEnabled(false);
  }

  // Matter Event Callback
  Matter.onEvent([](matterEvent_t event, const chip::DeviceLayer::ChipDeviceEvent *deviceEvent) {
    switch (event) {
      case MATTER_COMMISSIONING_COMPLETE:
        Serial.println("[Matter] Commissioning Complete! Joined Matter fabric.");
        ui.matterCommissioned = true;
        ui.needsFullRedraw = true;
        // Stop BLE advertising cleanly once commissioned
        chip::DeviceLayer::ConnectivityMgr().SetBLEAdvertisingEnabled(false);
        break;
      case MATTER_WIFI_CONNECTIVITY_CHANGE:
        Serial.println("[Matter] Wi-Fi Connectivity Changed.");
        if (WiFi.status() == WL_CONNECTED) {
          WiFi.setTxPower(WIFI_POWER_13dBm);
          esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
          wifi_config_t conf;
          if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
            if (conf.sta.listen_interval != 10) {
              conf.sta.listen_interval = 10;
              esp_wifi_set_config(WIFI_IF_STA, &conf);
            }
          }
        }
        break;
      case MATTER_INTERFACE_IP_ADDRESS_CHANGED:
        Serial.printf("[Matter] IP Address: %s\n", WiFi.localIP().toString().c_str());
        ui.ipAddr = WiFi.localIP().toString();
        ui.needsFullRedraw = true;
        if (WiFi.status() == WL_CONNECTED) {
          WiFi.setTxPower(WIFI_POWER_13dBm);
          esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
          wifi_config_t conf;
          if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
            if (conf.sta.listen_interval != 10) {
              conf.sta.listen_interval = 10;
              esp_wifi_set_config(WIFI_IF_STA, &conf);
            }
          }
        }
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

  // --- Fast IMU Polling (100ms) & Accelerometer Auto-Orientation (ONLY WHEN SCREEN IS ON) ---
  if (ui.displayOn && (now - lastImuPoll >= IMU_INTERVAL)) {
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
        ui.updateDisplay();
      } else if (ui.ax < -0.35f && ui.rotation != 3) {
        ui.rotation = 3;
        M5.Display.setRotation(3);
        ui.needsFullRedraw = true;
        ui.updateDisplay();
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
      ui.updateDisplay();
      lastActivityTime = now;
      pendingBtnASingleClick = false;
      delay(50);
    }
  } else {
    // Check Auto-Sleep (15s inactivity)
    if (now - lastActivityTime >= AUTO_SLEEP_TIMEOUT) {
      ui.sleepDisplay();
      pendingBtnASingleClick = false;
    }
    // Btn A: Single click = next view, Double click = prev view
    else if (btnAPressed && !isHoldingReset) {
      lastActivityTime = now;
      if (pendingBtnASingleClick) {
        if (now - lastBtnAPressTime < DOUBLE_CLICK_TIME) {
          pendingBtnASingleClick = false;
          ui.stepViewBack();
          ui.updateDisplay();
        } else {
          ui.stepView();
          ui.updateDisplay();
          pendingBtnASingleClick = true;
          lastBtnAPressTime = now;
        }
      } else {
        pendingBtnASingleClick = true;
        lastBtnAPressTime = now;
      }
    }
    // Btn B on Views 0-7: Manual toggle display sleep
    else if (btnBPressed && ui.currentView != VIEW_MATTER) {
      pendingBtnASingleClick = false;
      ui.sleepDisplay();
      lastActivityTime = now;
    }

    // Fire pending Btn A single click if double click window expired
    if (pendingBtnASingleClick && (now - lastBtnAPressTime >= DOUBLE_CLICK_TIME)) {
      pendingBtnASingleClick = false;
      ui.stepView();
      ui.updateDisplay();
    }
  }

  // --- Matter Factory Reset on View 8 (Hold Button B for 4s) ---
  if (ui.currentView == VIEW_MATTER && ui.displayOn) {
    if (btnBIsPressed) {
      pendingBtnASingleClick = false;
      lastActivityTime = now; // Keep display awake while holding
      if (btnBHoldStart == 0) {
        btnBHoldStart = now;
        lastReportedRemaining = -1;
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
        } else if (remaining != lastReportedRemaining) {
          lastReportedRemaining = remaining;
          ui.drawResetCountdown(remaining);
        }
      }
    } else {
      if (btnBHoldStart != 0) {
        btnBHoldStart = 0;
        lastReportedRemaining = -1;
        if (isHoldingReset) {
          isHoldingReset = false;
          ui.needsFullRedraw = true;
          ui.updateDisplay();
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

      // 4. Update Matter Endpoints (only when changed beyond deadband to save RF traffic)
      if (Matter.isDeviceCommissioned()) {
        if (fabsf(ui.temp_c - lastReportedTemp) >= 0.1f) {
          matterTemp.setTemperature(ui.temp_c);
          lastReportedTemp = ui.temp_c;
        }
        if (fabsf(ui.humidity - lastReportedHum) >= 0.5f) {
          matterHum.setHumidity(ui.humidity);
          lastReportedHum = ui.humidity;
        }
        if (fabsf(ui.pressure - lastReportedPress) >= 0.5f) {
          matterPress.setPressure(ui.pressure);
          lastReportedPress = ui.pressure;
        }
      }
    }

    // 5. Estimate instantaneous power draw in mW
    float powerDraw = 0.0f;
    if (ui.isCharging) {
      powerDraw = 0.0f; // USB powered / battery charging
    } else {
      // Base ESP32-S3 (80 MHz) + M5PM1 PMIC + BMI270 IMU: ~22 mA
      float current_mA = 22.0f;

      // ST7789 display controller + backlight (brightness 60): ~17 mA
      if (ui.displayOn) {
        current_mA += 17.0f;
      }

      // Wi-Fi subsystem (listen_interval = 10 + MAX modem sleep):
      if (WiFi.status() == WL_CONNECTED) {
        current_mA += 8.0f; // Multi-beacon listening with MAX modem sleep
      } else if (Matter.isDeviceCommissioned()) {
        current_mA += 60.0f; // Wi-Fi reconnect / scan active
      } else {
        current_mA += 30.0f; // BLE advertising for commissioning
      }

      // BME688 gas sensor heater pulse:
      if (shouldReadGas) {
        current_mA += 1.0f; // 15 mA for 80 ms averaged over 60s polling interval
      }

      // Power (mW) = Voltage (V) * Current (mA)
      powerDraw = (ui.vbat / 1000.0f) * current_mA;
    }

    // Append to on-screen historical chart buffers (all 6 metrics)
    ui.appendHistory(ui.temp_c, ui.humidity, ui.pressure, ui.gas_res, (float)ui.batLevel, powerDraw);

    // Update Matter & Wi-Fi status
    ui.matterCommissioned = Matter.isDeviceCommissioned();
    ui.matterConnected = Matter.isDeviceConnected();
    if (WiFi.status() == WL_CONNECTED) {
      ui.ipAddr = WiFi.localIP().toString();
      ui.wifiSSID = WiFi.SSID();
      WiFi.setTxPower(WIFI_POWER_13dBm);
      esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
      wifi_config_t conf;
      if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
        if (conf.sta.listen_interval != 10) {
          conf.sta.listen_interval = 10;
          esp_wifi_set_config(WIFI_IF_STA, &conf);
        }
      }
    }

    // Refresh display if screen is ON and not in reset countdown
    if (ui.displayOn && !isHoldingReset) {
      ui.updateDisplay();
    }
  }

  delay(ui.displayOn ? 20 : 100);
}
