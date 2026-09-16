"""
M5Stack StickS3 + ENV Pro (BME688) Sensor Dashboard & Logger
------------------------------------------------------------
Features:
- View 1: Stick S3 Internal Sensors (BMI270 6-Axis IMU, Spirit Level, Battery/Power)
- View 2: ENV Pro Overview (Live 4-tile numerical dashboard)
- Views 3-6: Dedicated Historical Time-Series Charts for ENV Pro metrics:
    * Temperature (°C)
    * Relative Humidity (%RH)
    * Barometric Pressure (hPa)
    * Gas Resistance (kΩ)
- Background sensor polling every 2 seconds continuously, even when display is OFF.
- Auto-sleep / power save: Screen backlight turns off after 30s of inactivity,
  while sensor polling continues unhindered in the background.

Controls:
- Front Button A: Step through 6 views:
    StickS3 -> Overview -> Temp Chart -> Humidity Chart -> Pressure Chart -> Gas Chart -> StickS3
- Front Button A (Hold 2.5s): Exit app back to UIFlow 2 menu (restores portrait mode)
- Side Button B (Short Press): Flip display 180 degrees
- Side Button B (Long Press):  Toggle display sleep/wake manually
"""

import time
import M5
from M5 import Display
from machine import Pin, SoftI2C
import unit

# Colors (Hex / RGB565)
BLACK       = 0x000000
DARKGREY    = 0x242424
MIDGREY     = 0x484848
LIGHTGREY   = 0x909090
WHITE       = 0xFFFFFF
RED         = 0xFF3333
GREEN       = 0x33FF33
CYAN        = 0x00FFFF
BLUE        = 0x3399FF
MAGENTA     = 0xFF33FF
YELLOW      = 0xFFFF33
ORANGE      = 0xFFA500

DEFAULT_BRIGHTNESS = 127
AUTO_SLEEP_MS      = 30000   # 30 seconds inactivity turns screen off
UPDATE_INTERVAL_MS = 2000    # 2 seconds sensor interval
MAX_HISTORY_POINTS = 120     # 120 points @ 2s = 4 minutes of history

VIEW_STICKS3     = 0
VIEW_OVERVIEW    = 1
VIEW_TEMP_CHART  = 2
VIEW_HUM_CHART   = 3
VIEW_PRESS_CHART = 4
VIEW_GAS_CHART   = 5
NUM_VIEWS        = 6

class SensorHistoryApp:
    def __init__(self):
        self.current_view = VIEW_STICKS3
        self.rotation = 1           # 1: Landscape, 3: Inverted Landscape (180 deg)
        self.display_on = True
        self.needs_full_redraw = True

        # Stick S3 internal readings
        self.accel = [0.0, 0.0, 0.0]
        self.gyro = [0.0, 0.0, 0.0]
        self.vbat = 0
        self.bat_level = 0
        self.is_charging = False

        # ENV Pro live readings
        self.temp_c = 0.0
        self.humidity = 0.0
        self.pressure = 0.0
        self.gas_res = 0.0

        # Time-series history buffers (ring buffers)
        self.history_temp = []
        self.history_hum = []
        self.history_press = []
        self.history_gas = []

        self.env_sensor = None
        self.env_connected = False
        self.i2c = None

        self.last_sensor_sample_ms = 0
        self.last_activity_ms = 0
        self.running = True

    def init_hardware(self):
        M5.begin()
        Display.setRotation(self.rotation)
        Display.setBrightness(DEFAULT_BRIGHTNESS)
        Display.fillScreen(BLACK)

        # Splash screen
        Display.setTextColor(CYAN, BLACK)
        Display.setTextSize(2)
        Display.drawString("StickS3 + ENV", 25, 30)
        Display.setTextColor(LIGHTGREY, BLACK)
        Display.setTextSize(1)
        Display.drawString("Initializing 6-View Dashboard...", 15, 70)

        # Keep Grove 5V enabled continuously for background polling
        try:
            M5.Power.setExtOutput(True)
        except Exception:
            pass
        time.sleep(0.3)

        # Initialize ENV Pro via isolated SoftI2C
        try:
            self.i2c = SoftI2C(
                sda=Pin(9, Pin.IN, Pin.PULL_UP),
                scl=Pin(10, Pin.IN, Pin.PULL_UP),
                freq=100000
            )
            scan_addrs = self.i2c.scan()
            if 0x77 in scan_addrs or 0x76 in scan_addrs:
                self.env_sensor = unit.ENVPROUnit(self.i2c)
                self.env_connected = True
                print("ENV Pro (BME688) initialized on Grove port (0x77)")
            else:
                print(f"Grove I2C scanned devices: {[hex(a) for a in scan_addrs]}")
        except Exception as e:
            print(f"ENV Pro init error: {e}")

        self.last_activity_ms = time.ticks_ms()
        self.last_sensor_sample_ms = time.ticks_ms() - UPDATE_INTERVAL_MS
        self.needs_full_redraw = True

    def sleep_display(self):
        if not self.display_on:
            return
        Display.setBrightness(0)
        self.display_on = False
        print("Screen OFF (Background sensor polling continues)")

    def wake_display(self):
        if self.display_on:
            self.last_activity_ms = time.ticks_ms()
            return
        Display.setBrightness(DEFAULT_BRIGHTNESS)
        self.display_on = True
        self.last_activity_ms = time.ticks_ms()
        self.needs_full_redraw = True
        print("Screen ON (Display refreshed)")

    def step_view(self):
        self.current_view = (self.current_view + 1) % NUM_VIEWS
        self.needs_full_redraw = True
        self.last_activity_ms = time.ticks_ms()

    def toggle_rotation(self):
        self.rotation = 3 if self.rotation == 1 else 1
        Display.setRotation(self.rotation)
        self.needs_full_redraw = True
        self.last_activity_ms = time.ticks_ms()

    def poll_sensors(self):
        # 1. IMU (BMI270)
        try:
            self.accel = M5.Imu.getAccel()
            self.gyro = M5.Imu.getGyro()
        except Exception:
            pass

        # 2. Battery & Power (PMIC)
        try:
            self.vbat = M5.Power.getBatteryVoltage()
            self.bat_level = M5.Power.getBatteryLevel()
            self.is_charging = M5.Power.isCharging()
        except Exception:
            pass

        # 3. ENV Pro (BME688)
        if not self.env_connected:
            return

        try:
            self.temp_c = self.env_sensor.get_temperature()
            self.humidity = self.env_sensor.get_humidity()
            self.pressure = self.env_sensor.get_pressure()
            self.gas_res = self.env_sensor.get_gas_resistance()

            # Append to history buffers
            self.history_temp.append(self.temp_c)
            self.history_hum.append(self.humidity)
            self.history_press.append(self.pressure)
            self.history_gas.append(self.gas_res)

            # Cap buffers at MAX_HISTORY_POINTS
            if len(self.history_temp) > MAX_HISTORY_POINTS:
                self.history_temp.pop(0)
                self.history_hum.pop(0)
                self.history_press.pop(0)
                self.history_gas.pop(0)

        except Exception as e:
            print(f"ENV read error: {e}")

    def draw_header(self, title_text, title_color):
        Display.fillRect(0, 0, 240, 20, DARKGREY)
        Display.setTextSize(1)
        Display.setTextColor(title_color, DARKGREY)
        Display.drawString(title_text, 6, 6)

        # Battery / Charge Status
        chg_str = "CHG" if self.is_charging else "BAT"
        bat_color = GREEN if self.bat_level > 25 else RED
        Display.setTextColor(bat_color, DARKGREY)
        Display.drawString(f"{self.bat_level}% {chg_str}", 175, 6)

    def draw_footer(self, view_index):
        Display.fillRect(0, 122, 240, 13, 0x181818)
        Display.setTextSize(1)
        Display.setTextColor(LIGHTGREY, 0x181818)
        Display.drawString(f"[{view_index+1}/{NUM_VIEWS}] BtnA:Next | BtnB:Flip", 16, 124)

    # --- View 0: Stick S3 Internal Sensors ---
    def draw_view_sticks3_static(self):
        Display.fillScreen(BLACK)
        self.draw_header("STICK S3 SENSORS", CYAN)
        self.draw_footer(VIEW_STICKS3)

        # Motion Panel Box (Left)
        Display.drawRoundRect(4, 23, 142, 96, 4, MIDGREY)
        Display.setTextColor(YELLOW, BLACK)
        Display.setTextSize(1)
        Display.drawString("BMI270 IMU", 12, 27)

        # Static labels for Accel
        Display.setTextColor(LIGHTGREY, BLACK)
        Display.drawString("AX:", 12, 42)
        Display.drawString("AY:", 12, 54)
        Display.drawString("AZ:", 12, 66)

        # Static labels for Gyro
        Display.drawString("GX:", 78, 42)
        Display.drawString("GY:", 78, 54)
        Display.drawString("GZ:", 78, 66)

        # Spirit Level Box (Right Top)
        Display.drawRoundRect(150, 23, 86, 56, 4, MIDGREY)
        Display.setTextColor(CYAN, BLACK)
        Display.drawString("LEVEL", 172, 27)
        Display.drawCircle(193, 53, 16, 0x303030)
        Display.drawCircle(193, 53, 6, 0x404040)
        Display.drawPixel(193, 53, WHITE)

        # Power Panel Box (Right Bottom)
        Display.drawRoundRect(150, 82, 86, 37, 4, MIDGREY)
        Display.setTextColor(LIGHTGREY, BLACK)
        Display.drawString("VBat:", 156, 88)
        Display.drawString("State:", 156, 102)

    def draw_view_sticks3_dynamic(self):
        Display.setTextSize(1)

        # 1. Update Accel readings
        Display.setTextColor(WHITE, BLACK)
        Display.drawString(f"{self.accel[0]:+5.2f}g", 34, 42)
        Display.drawString(f"{self.accel[1]:+5.2f}g", 34, 54)
        Display.drawString(f"{self.accel[2]:+5.2f}g", 34, 66)

        # 2. Update Gyro readings
        Display.drawString(f"{self.gyro[0]:+5.0f}d", 100, 42)
        Display.drawString(f"{self.gyro[1]:+5.0f}d", 100, 54)
        Display.drawString(f"{self.gyro[2]:+5.0f}d", 100, 66)

        # 3. Dynamic Tilt / Spirit Level Indicator
        cx, cy = 193, 53
        Display.fillCircle(cx, cy, 14, BLACK)
        Display.drawCircle(cx, cy, 14, 0x303030)
        Display.drawCircle(cx, cy, 6, 0x404040)
        Display.drawPixel(cx, cy, WHITE)

        bx = int(cx + max(-12, min(12, self.accel[1] * 12)))
        by = int(cy - max(-12, min(12, self.accel[0] * 12)))
        bubble_color = GREEN if (abs(bx - cx) <= 2 and abs(by - cy) <= 2) else YELLOW
        Display.fillCircle(bx, by, 3, bubble_color)

        # 4. Power Panel
        Display.setTextColor(WHITE, BLACK)
        Display.drawString(f"{self.vbat}mV", 192, 88)
        chg_text = "CHARGING" if self.is_charging else "BATTERY "
        chg_col = GREEN if self.is_charging else ORANGE
        Display.setTextColor(chg_col, BLACK)
        Display.drawString(chg_text, 192, 102)

    # --- View 1: ENV Pro Overview Dashboard ---
    def draw_overview_static(self):
        Display.fillScreen(BLACK)
        self.draw_header("ENV PRO OVERVIEW", GREEN)
        self.draw_footer(VIEW_OVERVIEW)

        if not self.env_connected:
            Display.setTextColor(RED, BLACK)
            Display.setTextSize(1)
            Display.drawString("ENV PRO (BME688) NOT DETECTED", 20, 50)
            Display.drawString("Port A: SDA=GPIO 9, SCL=GPIO 10", 20, 70)
            return

        # 4 Sensor Tiles (2x2 Grid)
        Display.drawRoundRect(4, 23, 114, 46, 4, MIDGREY)
        Display.setTextColor(ORANGE, BLACK)
        Display.setTextSize(1)
        Display.drawString("TEMPERATURE", 10, 27)

        Display.drawRoundRect(122, 23, 114, 46, 4, MIDGREY)
        Display.setTextColor(CYAN, BLACK)
        Display.drawString("REL. HUMIDITY", 128, 27)

        Display.drawRoundRect(4, 72, 114, 46, 4, MIDGREY)
        Display.setTextColor(GREEN, BLACK)
        Display.drawString("BARO PRESSURE", 10, 76)

        Display.drawRoundRect(122, 72, 114, 46, 4, MIDGREY)
        Display.setTextColor(MAGENTA, BLACK)
        Display.drawString("GAS RESIST", 128, 76)

    def draw_overview_dynamic(self):
        if not self.env_connected:
            return

        # Tile 1: Temp
        Display.setTextSize(2)
        temp_color = CYAN if self.temp_c < 20.0 else (GREEN if self.temp_c < 28.0 else ORANGE)
        Display.setTextColor(temp_color, BLACK)
        Display.drawString(f"{self.temp_c:5.1f} C", 14, 44)

        # Tile 2: Humidity
        Display.setTextColor(WHITE, BLACK)
        Display.drawString(f"{self.humidity:5.1f} %", 132, 44)

        # Tile 3: Pressure
        Display.setTextSize(1)
        Display.setTextColor(GREEN, BLACK)
        Display.drawString(f"{self.pressure:7.1f} hPa", 10, 96)

        # Tile 4: Gas Resistance
        Display.setTextColor(MAGENTA, BLACK)
        Display.drawString(f"{self.gas_res:6.2f} kOhm", 128, 96)

    # --- Views 2-5: Generic Time-Series Chart Component ---
    def draw_chart(self, title, unit_str, color, data_buf, view_index, min_span=1.0):
        Display.fillScreen(BLACK)
        self.draw_header(f"{title} ({unit_str})", color)
        self.draw_footer(view_index)

        # Chart Box Dimensions
        gx = 40       # Left edge of graph
        gy = 24       # Top edge of graph
        gw = 196      # Width of graph
        gh = 92       # Height of graph

        Display.drawRect(gx, gy, gw, gh, MIDGREY)

        if not data_buf:
            Display.setTextColor(LIGHTGREY, BLACK)
            Display.setTextSize(1)
            Display.drawString("Collecting data...", gx + 40, gy + 38)
            return

        cur_val = data_buf[-1]
        v_min = min(data_buf)
        v_max = max(data_buf)

        # Ensure minimum span
        if (v_max - v_min) < min_span:
            v_mid = (v_max + v_min) / 2.0
            v_min = v_mid - (min_span / 2.0)
            v_max = v_mid + (min_span / 2.0)

        # Stats readout: Now, Min, Max
        Display.setTextSize(1)
        Display.setTextColor(WHITE, BLACK)
        stats_str = f"Now:{cur_val:5.1f}  Min:{min(data_buf):5.1f}  Max:{max(data_buf):5.1f}"
        Display.drawString(stats_str, gx + 4, gy + 4)

        # Y-Axis labels (Left)
        Display.setTextColor(LIGHTGREY, BLACK)
        Display.drawString(f"{v_max:5.1f}", 2, gy + 4)
        Display.drawString(f"{((v_max+v_min)/2):5.1f}", 2, gy + (gh // 2) - 3)
        Display.drawString(f"{v_min:5.1f}", 2, gy + gh - 9)

        # Midline grid
        mid_y = gy + (gh // 2)
        Display.drawLine(gx + 1, mid_y, gx + gw - 2, mid_y, 0x1A1A1A)

        n_points = len(data_buf)
        if n_points < 2:
            return

        x_step = float(gw - 4) / float(MAX_HISTORY_POINTS - 1)
        start_offset = MAX_HISTORY_POINTS - n_points

        prev_px = None
        prev_py = None

        for idx, val in enumerate(data_buf):
            px = int(gx + 2 + (start_offset + idx) * x_step)
            norm = (val - v_min) / (v_max - v_min)
            norm = max(0.0, min(1.0, norm))
            py = int((gy + gh - 4) - norm * (gh - 22))

            if prev_px is not None:
                Display.drawLine(prev_px, prev_py, px, py, color)
                Display.drawLine(prev_px, prev_py + 1, px, py + 1, color)

            prev_px = px
            prev_py = py

        if prev_px is not None:
            Display.fillCircle(prev_px, prev_py, 2, WHITE)

    def update_display(self):
        if not self.display_on:
            return

        if self.current_view == VIEW_STICKS3:
            if self.needs_full_redraw:
                self.draw_view_sticks3_static()
                self.needs_full_redraw = False
            self.draw_view_sticks3_dynamic()

        elif self.current_view == VIEW_OVERVIEW:
            if self.needs_full_redraw:
                self.draw_overview_static()
                self.needs_full_redraw = False
            self.draw_overview_dynamic()

        elif self.current_view == VIEW_TEMP_CHART:
            self.draw_chart("TEMPERATURE", "°C", ORANGE, self.history_temp, VIEW_TEMP_CHART, min_span=1.0)
            self.needs_full_redraw = False

        elif self.current_view == VIEW_HUM_CHART:
            self.draw_chart("HUMIDITY", "%", CYAN, self.history_hum, VIEW_HUM_CHART, min_span=2.0)
            self.needs_full_redraw = False

        elif self.current_view == VIEW_PRESS_CHART:
            self.draw_chart("PRESSURE", "hPa", GREEN, self.history_press, VIEW_PRESS_CHART, min_span=1.0)
            self.needs_full_redraw = False

        elif self.current_view == VIEW_GAS_CHART:
            self.draw_chart("GAS RESIST", "kΩ", MAGENTA, self.history_gas, VIEW_GAS_CHART, min_span=1.0)
            self.needs_full_redraw = False

    def run(self):
        self.init_hardware()

        try:
            while self.running:
                M5.update()
                now_ms = time.ticks_ms()

                btn_a_pressed = M5.BtnA.wasPressed()
                btn_a_hold    = M5.BtnA.wasHold()
                btn_b_pressed = M5.BtnB.wasPressed()
                btn_b_hold    = M5.BtnB.wasHold()

                # --- Exit to UIFlow2 Menu (Hold Front Button A for 2.5s) ---
                if btn_a_hold and self.display_on:
                    print("Exiting app back to UIFlow 2 menu...")
                    Display.fillScreen(BLACK)
                    Display.setTextColor(WHITE, BLACK)
                    Display.drawString("Returning to menu...", 20, 50)
                    time.sleep(0.5)
                    self.running = False
                    break

                # --- Handle Wakeup from Sleep ---
                if not self.display_on:
                    if btn_a_pressed or btn_b_pressed:
                        self.wake_display()
                        time.sleep_ms(50)

                else:
                    # Inactivity Auto-Sleep Check (30 seconds)
                    if time.ticks_diff(now_ms, self.last_activity_ms) >= AUTO_SLEEP_MS:
                        self.sleep_display()

                    # Manual Sleep Toggle (Hold Side Button B for > 1s)
                    elif btn_b_hold:
                        self.sleep_display()

                    # Front Button A: Step through Views
                    elif btn_a_pressed:
                        self.step_view()

                    # Side Button B: Toggle 180 deg screen flip
                    elif btn_b_pressed:
                        self.toggle_rotation()

                # --- 2-Second Periodic Sensor Update & Polling ---
                if time.ticks_diff(now_ms, self.last_sensor_sample_ms) >= UPDATE_INTERVAL_MS:
                    self.poll_sensors()
                    self.last_sensor_sample_ms = now_ms

                    # If screen is active, update current view
                    if self.display_on:
                        self.update_display()

                time.sleep_ms(20)

        finally:
            print("Restoring portrait display orientation (rotation 0)...")
            try:
                Display.setRotation(0)
                Display.setBrightness(DEFAULT_BRIGHTNESS)
                Display.fillScreen(BLACK)
            except Exception:
                pass

if __name__ == "__main__":
    app = SensorHistoryApp()
    app.run()
