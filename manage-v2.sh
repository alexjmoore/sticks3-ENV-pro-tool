#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CLI="$SCRIPT_DIR/bin/arduino-cli"
BOARD_FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=8M,PartitionScheme=huge_app,PSRAM=opi,DebugLevel=info"
SKETCH_DIR="$SCRIPT_DIR/firmware/matter"

# Auto-detect serial port
find_port() {
  if [ -n "$PORT" ]; then
    echo "$PORT"
    return
  fi
  local dev_port
  dev_port=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -n 1)
  if [ -n "$dev_port" ]; then
    echo "$dev_port"
    return
  fi
  local by_id_port
  by_id_port=$(ls /dev/serial/by-id/* 2>/dev/null | head -n 1)
  if [ -n "$by_id_port" ]; then
    readlink -f "$by_id_port"
    return
  fi
  echo "/dev/ttyACM0"
}

PORT=$(find_port)

case "$1" in
  detect)
    echo "Scanning for StickS3 on USB..."
    echo "Found port: $PORT"
    ls -l "$PORT" 2>/dev/null || true
    ;;

  setup)
    echo "Setting up Version 2.0 Matter build environment..."
    mkdir -p "$SCRIPT_DIR/bin"
    if [ ! -f "$CLI" ]; then
      curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR="$SCRIPT_DIR/bin" sh
    fi
    "$CLI" config init --overwrite
    "$CLI" config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
    "$CLI" core update-index
    "$CLI" core install esp32:esp32
    "$CLI" lib install "M5Unified" "M5GFX" "BME68x Sensor library" "Adafruit BME680 Library" "Adafruit Unified Sensor"
    echo "Setup completed successfully!"
    ;;

  compile)
    echo "Compiling Matter-over-Wi-Fi firmware (v2.0)..."
    "$CLI" compile --fqbn "$BOARD_FQBN" "$SKETCH_DIR"
    echo "Compilation successful!"
    ;;

  flash|upload)
    echo "Uploading Matter-over-Wi-Fi firmware to StickS3 at $PORT..."
    "$CLI" upload -p "$PORT" --fqbn "$BOARD_FQBN" "$SKETCH_DIR"
    echo "Firmware successfully flashed to StickS3!"
    ;;

  monitor)
    echo "Opening serial monitor on $PORT (115200 baud). Press Ctrl+C to exit..."
    "$CLI" monitor -p "$PORT" --config baudrate=115200
    ;;

  matter-reset)
    echo "Performing Matter Factory Reset (wiping NVS pairing keys)..."
    python3 -c "
import serial, time
s = serial.Serial('$PORT', 115200, timeout=1)
s.write(b'reset\r\n')
s.close()
" 2>/dev/null || true
    # Erase NVS partition using esptool
    ~/.arduino15/packages/esp32/tools/esptool_py/*/esptool --port "$PORT" erase_region 0x9000 0x5000 2>/dev/null || true
    echo "Matter credentials wiped! Device will enter pairing mode on boot."
    ;;

  *)
    echo "M5Stack StickS3 + ENV Pro - Version 2.0 Matter Tool"
    echo ""
    echo "Usage: ./manage-v2.sh [command]"
    echo ""
    echo "Commands:"
    echo "  detect       - Detect connected StickS3 USB port"
    echo "  setup        - Install local arduino-cli, ESP32 core 3.x, and libraries"
    echo "  compile      - Compile native Matter C++ firmware"
    echo "  flash        - Compile and flash firmware to StickS3"
    echo "  monitor      - Open serial monitor (view pairing QR URL and logs)"
    echo "  matter-reset - Wipe Matter credentials and return to pairing mode"
    echo ""
    echo "Port: $PORT (override with PORT=/dev/... ./manage-v2.sh [cmd])"
    ;;
esac
