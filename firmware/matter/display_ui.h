#include <M5Unified.h>
#include <lgfx/utility/lgfx_qrcode.h>


#define MAX_HISTORY 120
#define NUM_VIEWS 9

enum ViewType {
  VIEW_STICKS3 = 0,
  VIEW_OVERVIEW = 1,
  VIEW_TEMP_CHART = 2,
  VIEW_HUM_CHART = 3,
  VIEW_PRESS_CHART = 4,
  VIEW_GAS_CHART = 5,
  VIEW_BAT_CHART = 6,
  VIEW_POWER_CHART = 7,
  VIEW_MATTER = 8
};

class DisplayUI {
public:
  int currentView = VIEW_STICKS3;
  int rotation = 1;
  bool displayOn = true;
  bool needsFullRedraw = true;
  uint8_t brightness = 60;

  // Sensor cached data
  float ax = 0, ay = 0, az = 0;
  float gx = 0, gy = 0, gz = 0;
  int batLevel = 100;
  int vbat = 4100;
  bool isCharging = false;

  float temp_c = 0;
  float humidity = 0;
  float pressure = 0;
  float gas_res = 0;

  // History buffers
  float histTemp[MAX_HISTORY];
  float histHum[MAX_HISTORY];
  float histPress[MAX_HISTORY];
  float histGas[MAX_HISTORY];
  float histBat[MAX_HISTORY];
  float histPower[MAX_HISTORY];
  int histCount = 0;

  // Matter state
  bool matterCommissioned = false;
  bool matterConnected = false;
  String wifiSSID = "";
  String ipAddr = "";
  String qrPayload = "";
  String manualCode = "";

  void init() {
    M5.Display.setRotation(rotation);
    M5.Display.setBrightness(brightness);
    M5.Display.fillScreen(TFT_BLACK);
  }

  void appendHistory(float t, float h, float p, float g, float bat, float pwr) {
    if (histCount < MAX_HISTORY) {
      histTemp[histCount] = t;
      histHum[histCount] = h;
      histPress[histCount] = p;
      histGas[histCount] = g;
      histBat[histCount] = bat;
      histPower[histCount] = pwr;
      histCount++;
    } else {
      for (int i = 0; i < MAX_HISTORY - 1; i++) {
        histTemp[i] = histTemp[i + 1];
        histHum[i] = histHum[i + 1];
        histPress[i] = histPress[i + 1];
        histGas[i] = histGas[i + 1];
        histBat[i] = histBat[i + 1];
        histPower[i] = histPower[i + 1];
      }
      histTemp[MAX_HISTORY - 1] = t;
      histHum[MAX_HISTORY - 1] = h;
      histPress[MAX_HISTORY - 1] = p;
      histGas[MAX_HISTORY - 1] = g;
      histBat[MAX_HISTORY - 1] = bat;
      histPower[MAX_HISTORY - 1] = pwr;
    }
  }

  void stepView() {
    currentView = (currentView + 1) % NUM_VIEWS;
    needsFullRedraw = true;
  }

  void stepViewBack() {
    currentView = (currentView - 1 + NUM_VIEWS) % NUM_VIEWS;
    needsFullRedraw = true;
  }

  void toggleRotation() {
    rotation = (rotation == 1) ? 3 : 1;
    M5.Display.setRotation(rotation);
    needsFullRedraw = true;
  }

  void sleepDisplay() {
    if (!displayOn) return;
    M5.Display.setBrightness(0);
    M5.Display.sleep();
    displayOn = false;
  }

  void wakeDisplay() {
    if (displayOn) return;
    M5.Display.wakeup();
    M5.Display.setBrightness(brightness);
    displayOn = true;
    needsFullRedraw = true;
  }

  void drawHeader(const char* title, uint16_t color) {
    M5.Display.fillRect(0, 0, 240, 20, 0x2104);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(color, 0x2104);
    M5.Display.drawString(title, 6, 6);

    uint16_t batCol = (batLevel > 25) ? TFT_GREEN : TFT_RED;
    M5.Display.setTextColor(batCol, 0x2104);
    char batBuf[16];
    snprintf(batBuf, sizeof(batBuf), "%d%% %s", batLevel, isCharging ? "CHG" : "BAT");
    M5.Display.drawString(batBuf, 175, 6);
  }

  void drawFooter(int idx) {
    M5.Display.fillRect(0, 122, 240, 13, 0x18C3);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(0x9492, 0x18C3);
    char footBuf[48];
    snprintf(footBuf, sizeof(footBuf), "[%d/%d] A:Next (2xA:Prev) | B:Sleep", idx + 1, NUM_VIEWS);
    M5.Display.drawString(footBuf, 10, 124);
  }

  // View 0: Stick S3 Sensors
  void drawStickS3() {
    if (needsFullRedraw) {
      M5.Display.fillScreen(TFT_BLACK);
      drawHeader("STICK S3 SENSORS", TFT_CYAN);
      drawFooter(VIEW_STICKS3);

      M5.Display.drawRoundRect(4, 23, 142, 96, 4, 0x4A49);
      M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
      M5.Display.drawString("BMI270 IMU", 12, 27);

      M5.Display.setTextColor(0x9492, TFT_BLACK);
      M5.Display.drawString("AX:", 12, 42);
      M5.Display.drawString("AY:", 12, 54);
      M5.Display.drawString("AZ:", 12, 66);
      M5.Display.drawString("GX:", 78, 42);
      M5.Display.drawString("GY:", 78, 54);
      M5.Display.drawString("GZ:", 78, 66);

      M5.Display.drawRoundRect(150, 23, 86, 56, 4, 0x4A49);
      M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
      M5.Display.drawString("LEVEL", 172, 27);
      M5.Display.drawCircle(193, 53, 16, 0x3186);
      M5.Display.drawCircle(193, 53, 6, 0x4208);
      M5.Display.drawPixel(193, 53, TFT_WHITE);

      M5.Display.drawRoundRect(150, 80, 86, 39, 4, 0x4A49);
      M5.Display.setTextColor(0x9492, TFT_BLACK);
      M5.Display.drawString("VBat:", 154, 84);
      M5.Display.drawString("State:", 154, 95);
      M5.Display.drawString("Die:", 154, 106);

      needsFullRedraw = false;
    }

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    char buf[20];
    snprintf(buf, sizeof(buf), "%+5.2fg", ax); M5.Display.drawString(buf, 34, 42);
    snprintf(buf, sizeof(buf), "%+5.2fg", ay); M5.Display.drawString(buf, 34, 54);
    snprintf(buf, sizeof(buf), "%+5.2fg", az); M5.Display.drawString(buf, 34, 66);

    snprintf(buf, sizeof(buf), "%+5.0fd", gx); M5.Display.drawString(buf, 100, 42);
    snprintf(buf, sizeof(buf), "%+5.0fd", gy); M5.Display.drawString(buf, 100, 54);
    snprintf(buf, sizeof(buf), "%+5.0fd", gz); M5.Display.drawString(buf, 100, 66);

    int cx = 193, cy = 53;
    M5.Display.fillCircle(cx, cy, 14, TFT_BLACK);
    M5.Display.drawCircle(cx, cy, 14, 0x3186);
    M5.Display.drawCircle(cx, cy, 6, 0x4208);
    M5.Display.drawPixel(cx, cy, TFT_WHITE);

    int bx = cx + constrain((int)(ay * 12), -12, 12);
    int by = cy - constrain((int)(ax * 12), -12, 12);
    uint16_t bcol = (abs(bx - cx) <= 2 && abs(by - cy) <= 2) ? TFT_GREEN : TFT_YELLOW;
    M5.Display.fillCircle(bx, by, 3, bcol);

    snprintf(buf, sizeof(buf), "%dmV ", vbat);
    M5.Display.drawString(buf, 192, 84);
    M5.Display.setTextColor(isCharging ? TFT_GREEN : TFT_ORANGE, TFT_BLACK);
    M5.Display.drawString(isCharging ? "CHG " : "BAT ", 192, 95);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%4.1fC ", temperatureRead());
    M5.Display.drawString(buf, 192, 106);
  }

  // View 1: ENV Pro Overview
  void drawOverview() {
    if (needsFullRedraw) {
      M5.Display.fillScreen(TFT_BLACK);
      drawHeader("ENV PRO OVERVIEW", TFT_GREEN);
      drawFooter(VIEW_OVERVIEW);

      M5.Display.drawRoundRect(4, 23, 114, 46, 4, 0x4A49);
      M5.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
      M5.Display.drawString("TEMPERATURE", 10, 27);

      M5.Display.drawRoundRect(122, 23, 114, 46, 4, 0x4A49);
      M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
      M5.Display.drawString("REL. HUMIDITY", 128, 27);

      M5.Display.drawRoundRect(4, 72, 114, 46, 4, 0x4A49);
      M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
      M5.Display.drawString("BARO PRESSURE", 10, 76);

      M5.Display.drawRoundRect(122, 72, 114, 46, 4, 0x4A49);
      M5.Display.setTextColor(TFT_MAGENTA, TFT_BLACK);
      M5.Display.drawString("GAS RESIST", 128, 76);

      needsFullRedraw = false;
    }

    char buf[24];
    M5.Display.setTextSize(2);
    uint16_t tcol = (temp_c < 20.0) ? TFT_CYAN : ((temp_c < 28.0) ? TFT_GREEN : TFT_ORANGE);
    M5.Display.setTextColor(tcol, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%5.1f C", temp_c);
    M5.Display.drawString(buf, 14, 44);

    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%5.1f %%", humidity);
    M5.Display.drawString(buf, 132, 44);

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%7.1f hPa", pressure);
    M5.Display.drawString(buf, 10, 96);

    M5.Display.setTextColor(TFT_MAGENTA, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%6.2f kOhm", gas_res);
    M5.Display.drawString(buf, 128, 96);
  }

  // Views 2-5: Historical Charts
  void drawChart(const char* title, const char* unit, uint16_t color, float* data, int viewIdx, float minSpan) {
    M5.Display.fillScreen(TFT_BLACK);
    char head[32];
    snprintf(head, sizeof(head), "%s (%s)", title, unit);
    drawHeader(head, color);
    drawFooter(viewIdx);

    int gx = 40, gy = 24, gw = 196, gh = 92;
    M5.Display.drawRect(gx, gy, gw, gh, 0x4A49);

    if (histCount == 0) {
      M5.Display.setTextColor(0x9492, TFT_BLACK);
      M5.Display.drawString("Collecting data...", gx + 40, gy + 38);
      return;
    }

    float vMin = data[0], vMax = data[0];
    for (int i = 1; i < histCount; i++) {
      if (data[i] < vMin) vMin = data[i];
      if (data[i] > vMax) vMax = data[i];
    }
    float curVal = data[histCount - 1];

    if ((vMax - vMin) < minSpan) {
      float mid = (vMax + vMin) / 2.0f;
      vMin = mid - (minSpan / 2.0f);
      vMax = mid + (minSpan / 2.0f);
    }

    if (viewIdx == VIEW_BAT_CHART) {
      if (vMax > 100.0f) vMax = 100.0f;
      if (vMin < 0.0f) vMin = 0.0f;
    } else if (viewIdx == VIEW_POWER_CHART) {
      if (vMin < 0.0f) vMin = 0.0f;
    }

    char buf[36];
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    if (viewIdx == VIEW_BAT_CHART) {
      snprintf(buf, sizeof(buf), "Now:%3.0f%% (%dmV) Min:%3.0f%%", curVal, vbat, vMin);
    } else if (viewIdx == VIEW_POWER_CHART) {
      snprintf(buf, sizeof(buf), "Now:%3.0fmW (%dmV) Min:%3.0f", curVal, vbat, vMin);
    } else {
      snprintf(buf, sizeof(buf), "Now:%5.1f Min:%5.1f Max:%5.1f", curVal, vMin, vMax);
    }
    M5.Display.drawString(buf, gx + 4, gy + 4);

    M5.Display.setTextColor(0x9492, TFT_BLACK);
    if (viewIdx == VIEW_BAT_CHART || viewIdx == VIEW_POWER_CHART) {
      snprintf(buf, sizeof(buf), "%5.0f", vMax); M5.Display.drawString(buf, 2, gy + 4);
      snprintf(buf, sizeof(buf), "%5.0f", (vMax + vMin) / 2.0f); M5.Display.drawString(buf, 2, gy + (gh / 2) - 3);
      snprintf(buf, sizeof(buf), "%5.0f", vMin); M5.Display.drawString(buf, 2, gy + gh - 9);
    } else {
      snprintf(buf, sizeof(buf), "%5.1f", vMax); M5.Display.drawString(buf, 2, gy + 4);
      snprintf(buf, sizeof(buf), "%5.1f", (vMax + vMin) / 2.0f); M5.Display.drawString(buf, 2, gy + (gh / 2) - 3);
      snprintf(buf, sizeof(buf), "%5.1f", vMin); M5.Display.drawString(buf, 2, gy + gh - 9);
    }

    M5.Display.drawLine(gx + 1, gy + (gh / 2), gx + gw - 2, gy + (gh / 2), 0x18C3);

    if (histCount < 2) return;

    float xStep = (float)(gw - 4) / (float)(MAX_HISTORY - 1);
    int startOffset = MAX_HISTORY - histCount;

    int prevX = -1, prevY = -1;
    for (int i = 0; i < histCount; i++) {
      int px = gx + 2 + (int)((startOffset + i) * xStep);
      float norm = (data[i] - vMin) / (vMax - vMin);
      norm = constrain(norm, 0.0f, 1.0f);
      int py = (gy + gh - 4) - (int)(norm * (gh - 22));

      if (prevX >= 0) {
        M5.Display.drawLine(prevX, prevY, px, py, color);
        M5.Display.drawLine(prevX, prevY + 1, px, py + 1, color);
      }
      prevX = px; prevY = py;
    }

    if (prevX >= 0) {
      M5.Display.fillCircle(prevX, prevY, 2, TFT_WHITE);
    }
  }

  // View 8: Matter Status & Commissioning QR Code
  void drawMatterScreen() {
    M5.Display.fillScreen(TFT_BLACK);
    drawHeader("MATTER STATUS", TFT_YELLOW);
    drawFooter(VIEW_MATTER);

    // Left side: Render Matter Onboarding QR Code
    if (qrPayload.length() > 0) {
      String code = qrPayload;
      int dataIdx = code.indexOf("data=");
      if (dataIdx >= 0) {
        code = code.substring(dataIdx + 5);
      }
      M5.Display.qrcode(code.c_str(), 6, 23, 96, 1, true);
    }


    // Right side: Status and Manual Pairing Code
    int tx = 104;
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.drawString("Matter-over-WiFi", tx, 26);

    M5.Display.setTextColor(0x9492, TFT_BLACK);
    M5.Display.drawString("Pairing Code:", tx, 40);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString("3497-011-2332", tx, 52);

    M5.Display.setTextColor(0x9492, TFT_BLACK);
    M5.Display.drawString("Status:", tx, 66);
    if (matterCommissioned) {
      M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
      M5.Display.drawString("PAIRED (Fabric OK)", tx, 78);
      M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
      M5.Display.drawString(ipAddr.c_str(), tx, 90);
    } else {
      M5.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
      M5.Display.drawString("WAITING FOR BLE", tx, 78);
      M5.Display.setTextColor(0x9492, TFT_BLACK);
      M5.Display.drawString("Scan QR in Home App", tx, 90);
    }

    M5.Display.setTextColor(0x6B4D, TFT_BLACK);
    M5.Display.drawString("Hold Btn B 4s: Reset", tx, 106);
  }

  void drawResetCountdown(int secondsRemaining) {
    M5.Display.fillScreen(TFT_RED);
    M5.Display.setTextColor(TFT_WHITE, TFT_RED);
    M5.Display.setTextSize(2);
    M5.Display.drawString("FACTORY RESET", 30, 30);
    M5.Display.setTextSize(1);
    char buf[40];
    snprintf(buf, sizeof(buf), "Resetting Matter in %d...", secondsRemaining);
    M5.Display.drawString(buf, 40, 70);
    M5.Display.drawString("Release Btn B to cancel", 35, 95);
  }

  void updateDisplay() {
    if (!displayOn) return;

    switch (currentView) {
      case VIEW_STICKS3:
        drawStickS3();
        break;
      case VIEW_OVERVIEW:
        drawOverview();
        break;
      case VIEW_TEMP_CHART:
        drawChart("TEMPERATURE", "°C", TFT_ORANGE, histTemp, VIEW_TEMP_CHART, 1.0f);
        needsFullRedraw = false;
        break;
      case VIEW_HUM_CHART:
        drawChart("HUMIDITY", "%", TFT_CYAN, histHum, VIEW_HUM_CHART, 2.0f);
        needsFullRedraw = false;
        break;
      case VIEW_PRESS_CHART:
        drawChart("PRESSURE", "hPa", TFT_GREEN, histPress, VIEW_PRESS_CHART, 1.0f);
        needsFullRedraw = false;
        break;
      case VIEW_GAS_CHART:
        drawChart("GAS RESIST", "kΩ", TFT_MAGENTA, histGas, VIEW_GAS_CHART, 1.0f);
        needsFullRedraw = false;
        break;
      case VIEW_BAT_CHART:
        drawChart("BATTERY LEVEL", "%", TFT_YELLOW, histBat, VIEW_BAT_CHART, 5.0f);
        needsFullRedraw = false;
        break;
      case VIEW_POWER_CHART:
        drawChart("POWER DRAW", "mW", 0xFD20, histPower, VIEW_POWER_CHART, 30.0f);
        needsFullRedraw = false;
        break;
      case VIEW_MATTER:
        drawMatterScreen();
        needsFullRedraw = false;
        break;
    }
  }
};
