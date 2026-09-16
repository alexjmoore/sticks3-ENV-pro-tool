#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

PYTHON_BIN="$SCRIPT_DIR/.venv/bin/python3"
if [ ! -f "$PYTHON_BIN" ]; then
  if command -v uv >/dev/null 2>&1; then
    uv sync
  else
    echo "Virtual environment not found. Please run 'uv sync' first."
    exit 1
  fi
fi

if [ $# -eq 0 ]; then
  echo "M5Stack StickS3 + ENV Pro Management Tool"
  echo ""
  echo "Usage: ./manage.sh [command]"
  echo ""
  echo "Commands:"
  echo "  detect          - Detect connected StickS3 USB serial port"
  echo "  info            - Query StickS3 hardware, MicroPython & UIFlow info"
  echo "  run             - Run sensor dashboard live in RAM (src/main.py)"
  echo "  deploy          - Deploy to both :apps/sensor_dashboard.py and :main.py"
  echo "  deploy-app      - Install into :apps/sensor_dashboard.py (appears in APP LIST)"
  echo "  set-boot-menu   - Enable UIFlow 2 Startup Menu / APP LIST on boot"
  echo "  set-boot-direct - Enable direct boot into main.py"
  echo "  ls [dir]        - List files on device filesystem"
  echo "  cat <f>         - Read and display a file from the device"
  echo "  repl            - Open interactive MicroPython serial REPL"
  echo ""
  exit 0
fi

exec "$PYTHON_BIN" "$SCRIPT_DIR/tools/cli.py" "$@"
