#!/usr/bin/env python3
"""
Generate pixel-accurate preview screenshots of the StickS3 display views.
"""

import os
from PIL import Image, ImageDraw, ImageFont

# Colors
BLACK = (0, 0, 0)
DARKGREY = (36, 36, 36)
MIDGREY = (72, 72, 72)
LIGHTGREY = (144, 144, 144)
WHITE = (255, 255, 255)
RED = (255, 51, 51)
GREEN = (51, 255, 51)
CYAN = (0, 255, 255)
ORANGE = (255, 165, 0)
MAGENTA = (255, 51, 255)
YELLOW = (255, 255, 51)
BORDER_COL = (40, 40, 40)

def create_base_canvas():
    return Image.new("RGB", (240, 135), BLACK)

def draw_header(draw, title, color):
    draw.rectangle([0, 0, 240, 20], fill=DARKGREY)
    draw.text((6, 5), title, fill=color)
    draw.text((175, 5), "95% CHG", fill=GREEN)

def draw_footer(draw, view_idx):
    draw.rectangle([0, 122, 240, 134], fill=(24, 24, 24))
    draw.text((16, 124), f"[{view_idx}/6] BtnA:Next | BtnB:Flip", fill=LIGHTGREY)

def render_sticks3_view():
    img = create_base_canvas()
    d = ImageDraw.Draw(img)
    draw_header(d, "STICK S3 SENSORS", CYAN)
    draw_footer(d, 1)

    # Left Motion Panel
    d.rounded_rectangle([4, 23, 146, 119], radius=4, outline=MIDGREY)
    d.text((12, 27), "BMI270 IMU", fill=YELLOW)

    d.text((12, 44), "AX: +0.02g", fill=WHITE)
    d.text((12, 58), "AY: -0.15g", fill=WHITE)
    d.text((12, 72), "AZ: +0.98g", fill=WHITE)

    d.text((80, 44), "GX:  +0d", fill=WHITE)
    d.text((80, 58), "GY:  -1d", fill=WHITE)
    d.text((80, 72), "GZ:  +0d", fill=WHITE)

    # Right Spirit Level Box
    d.rounded_rectangle([150, 23, 236, 79], radius=4, outline=MIDGREY)
    d.text((172, 27), "LEVEL", fill=CYAN)
    cx, cy = 193, 53
    d.ellipse([cx-16, cy-16, cx+16, cy+16], outline=(48, 48, 48))
    d.ellipse([cx-6, cy-6, cx+6, cy+6], outline=(64, 64, 64))
    # Green bubble near center
    d.ellipse([cx-2, cy-2, cx+4, cy+4], fill=GREEN)

    # Right Power Box
    d.rounded_rectangle([150, 82, 236, 119], radius=4, outline=MIDGREY)
    d.text((156, 88), "VBat: 4150mV", fill=WHITE)
    d.text((156, 102), "State: CHARGING", fill=GREEN)

    return img.resize((480, 270), Image.NEAREST)

def render_overview_view():
    img = create_base_canvas()
    d = ImageDraw.Draw(img)
    draw_header(d, "ENV PRO OVERVIEW", GREEN)
    draw_footer(d, 2)

    # 4 Tiles
    # Tile 1: Temp
    d.rounded_rectangle([4, 23, 118, 69], radius=4, outline=MIDGREY)
    d.text((10, 27), "TEMPERATURE", fill=ORANGE)
    d.text((14, 44), " 26.2 C", fill=GREEN)

    # Tile 2: Humidity
    d.rounded_rectangle([122, 23, 236, 69], radius=4, outline=MIDGREY)
    d.text((128, 27), "REL. HUMIDITY", fill=CYAN)
    d.text((132, 44), " 46.5 %", fill=WHITE)

    # Tile 3: Pressure
    d.rounded_rectangle([4, 72, 118, 118], radius=4, outline=MIDGREY)
    d.text((10, 76), "BARO PRESSURE", fill=GREEN)
    d.text((12, 96), "1005.3 hPa", fill=GREEN)

    # Tile 4: Gas
    d.rounded_rectangle([122, 72, 236, 118], radius=4, outline=MIDGREY)
    d.text((128, 76), "GAS RESIST", fill=MAGENTA)
    d.text((128, 96), " 6.62 kOhm", fill=MAGENTA)

    return img.resize((480, 270), Image.NEAREST)

def render_chart_view(title, unit, color, min_val, max_val, cur_val, data_points, view_idx):
    img = create_base_canvas()
    d = ImageDraw.Draw(img)
    draw_header(d, f"{title} ({unit})", color)
    draw_footer(d, view_idx)

    gx, gy, gw, gh = 40, 24, 196, 92
    d.rectangle([gx, gy, gx+gw, gy+gh], outline=MIDGREY)

    d.text((gx+4, gy+4), f"Now:{cur_val:5.1f}  Min:{min_val:5.1f}  Max:{max_val:5.1f}", fill=WHITE)
    d.text((2, gy+4), f"{max_val:5.1f}", fill=LIGHTGREY)
    d.text((2, gy+(gh//2)-3), f"{(max_val+min_val)/2:5.1f}", fill=LIGHTGREY)
    d.text((2, gy+gh-9), f"{min_val:5.1f}", fill=LIGHTGREY)

    # Grid
    d.line([gx+1, gy+(gh//2), gx+gw-2, gy+(gh//2)], fill=(26, 26, 26))

    # Curve
    x_step = float(gw - 4) / float(len(data_points) - 1)
    pts = []
    for idx, val in enumerate(data_points):
        px = int(gx + 2 + idx * x_step)
        norm = (val - min_val) / (max_val - min_val)
        norm = max(0.0, min(1.0, norm))
        py = int((gy + gh - 4) - norm * (gh - 22))
        pts.append((px, py))

    for i in range(len(pts) - 1):
        d.line([pts[i], pts[i+1]], fill=color, width=2)

    # Indicator circle on last point
    last_x, last_y = pts[-1]
    d.ellipse([last_x-2, last_y-2, last_x+2, last_y+2], fill=WHITE)

    return img.resize((480, 270), Image.NEAREST)

def main():
    os.makedirs("docs/screenshots", exist_ok=True)

    # 1. Stick S3 View
    img1 = render_sticks3_view()
    img1.save("docs/screenshots/01_sticks3_imu.png")

    # 2. ENV Pro Overview View
    img2 = render_overview_view()
    img2.save("docs/screenshots/02_envpro_overview.png")

    # 3. Temp Chart
    temp_data = [24.8, 24.9, 25.0, 25.1, 25.2, 25.4, 25.8, 26.2, 26.5, 26.8, 26.7, 26.4, 26.2, 26.0, 25.9, 26.1, 26.2]
    img3 = render_chart_view("TEMPERATURE", "°C", ORANGE, 24.5, 27.0, 26.2, temp_data, 3)
    img3.save("docs/screenshots/03_temp_chart.png")

    # 4. Gas Chart
    gas_data = [5.4, 5.5, 5.7, 5.6, 5.8, 6.2, 6.8, 7.4, 7.9, 8.2, 8.0, 7.6, 7.1, 6.8, 6.6]
    img4 = render_chart_view("GAS RESIST", "kΩ", MAGENTA, 5.0, 8.5, 6.6, gas_data, 6)
    img4.save("docs/screenshots/04_gas_chart.png")

    print("Screenshots generated in docs/screenshots/:")
    for f in os.listdir("docs/screenshots"):
        print(f"  docs/screenshots/{f}")

if __name__ == "__main__":
    main()
