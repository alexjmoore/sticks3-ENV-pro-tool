#!/usr/bin/env python3
"""
M5Stack StickS3 CLI tool for managing, executing, and deploying MicroPython scripts.
"""

import sys
import os
import time
import glob
import serial

def find_port():
    env_port = os.environ.get("PORT")
    if env_port:
        return env_port
    by_id = glob.glob("/dev/serial/by-id/*")
    if by_id:
        return by_id[0]
    devs = glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*")
    if devs:
        return devs[0]
    return "/dev/ttyACM0"

class StickConnection:
    def __init__(self, port=None, baudrate=115200, timeout=3):
        self.port = port or find_port()
        self.baudrate = baudrate
        self.timeout = timeout
        self.ser = None

    def connect(self):
        self.ser = serial.Serial(self.port, self.baudrate, timeout=self.timeout)

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()

    def enter_raw_repl(self):
        # Send Ctrl+C multiple times to interrupt running loops / startup apps
        for _ in range(3):
            self.ser.write(b"\r\x03")
            time.sleep(0.1)
        self.ser.reset_input_buffer()

        # Enter raw REPL (Ctrl+A)
        self.ser.write(b"\r\x01")
        time.sleep(0.2)
        resp = self.ser.read_until(b"raw REPL; CTRL-B to exit\r\n>")
        if not resp.endswith(b"raw REPL; CTRL-B to exit\r\n>"):
            # Try once more
            self.ser.write(b"\r\x03\r\x01")
            time.sleep(0.3)
            resp = self.ser.read_until(b"raw REPL; CTRL-B to exit\r\n>")
            if not resp.endswith(b"raw REPL; CTRL-B to exit\r\n>"):
                raise RuntimeError(f"Could not enter raw REPL. Received: {resp}")
        return True

    def exit_raw_repl(self):
        # Ctrl+B exits raw REPL back to normal REPL
        self.ser.write(b"\r\x02")

    def exec_raw(self, code_bytes, timeout=10):
        self.enter_raw_repl()
        # Write code ending with Ctrl+D (EOT)
        self.ser.write(code_bytes + b"\x04")
        
        # Read response
        # In raw REPL, response format is:
        # OK<stdout>\x04<stderr>\x04>
        header = self.ser.read_until(b"OK")
        if not header.endswith(b"OK"):
            raise RuntimeError(f"Unexpected response header: {header}")

        # Read until first \x04 (stdout)
        stdout = self.ser.read_until(b"\x04")
        if stdout.endswith(b"\x04"):
            stdout = stdout[:-1]
            
        # Read until second \x04 (stderr)
        stderr = self.ser.read_until(b"\x04")
        if stderr.endswith(b"\x04"):
            stderr = stderr[:-1]

        # Read trailing '>'
        self.ser.read_until(b">")

        if stderr:
            raise RuntimeError(stderr.decode("utf-8", errors="replace"))

        return stdout.decode("utf-8", errors="replace")

    def run_file(self, filepath):
        with open(filepath, "rb") as f:
            code = f.read()
        self.enter_raw_repl()
        self.ser.write(code + b"\x04")
        # Follow live output
        header = self.ser.read_until(b"OK")
        print(f"--- Running {os.path.basename(filepath)} on StickS3 (Press Ctrl+C to stop) ---")
        try:
            while True:
                line = self.ser.readline()
                if not line:
                    continue
                if b"\x04" in line:
                    parts = line.split(b"\x04")
                    sys.stdout.write(parts[0].decode("utf-8", errors="replace"))
                    if len(parts) > 1 and parts[1]:
                        sys.stderr.write(parts[1].decode("utf-8", errors="replace"))
                    break
                sys.stdout.write(line.decode("utf-8", errors="replace"))
                sys.stdout.flush()
        except KeyboardInterrupt:
            print("\nInterrupted by user. Resetting...")
            self.ser.write(b"\r\x03\r\x03")
            time.sleep(0.2)

    def write_file(self, remote_path, content_bytes):
        chunk_size = 512
        # Use MicroPython to write file chunks
        cmd = f"f = open('{remote_path}', 'wb')\n".encode("utf-8")
        self.exec_raw(cmd)
        
        for i in range(0, len(content_bytes), chunk_size):
            chunk = content_bytes[i:i+chunk_size]
            chunk_repr = repr(chunk).encode("ascii")
            cmd = b"f.write(" + chunk_repr + b")\n"
            self.exec_raw(cmd)
            
        self.exec_raw(b"f.close()\n")
        print(f"Successfully uploaded {len(content_bytes)} bytes to :{remote_path}")

    def read_file(self, remote_path):
        code = f"""
with open('{remote_path}', 'r') as f:
    print(f.read(), end='')
""".encode("utf-8")
        return self.exec_raw(code)

    def list_files(self, path=""):
        code = f"""
import os
try:
    print(os.listdir('{path}'))
except Exception as e:
    print('Error:', e)
""".encode("utf-8")
        return self.exec_raw(code)


def main():
    if len(sys.argv) < 2:
        print("Usage: tools/cli.py <info|run|deploy|ls|cat|repl> [args]")
        sys.exit(1)

    cmd = sys.argv[1]

    if cmd == "detect":
        port = find_port()
        print(f"Connected StickS3 detected at: {port}")
        return

    conn = StickConnection()
    conn.connect()

    try:
        if cmd == "info":
            code = b"""
import sys, os, M5
print("=== StickS3 System Info ===")
print("MicroPython:", sys.version)
print("System:", os.uname())
print("M5 Version:", getattr(M5, '__version__', 'UIFlow2'))
print("Root directory:", os.listdir())
"""
            print(conn.exec_raw(code))

        elif cmd == "ls":
            path = sys.argv[2] if len(sys.argv) > 2 else ""
            print(conn.list_files(path))

        elif cmd == "cat":
            if len(sys.argv) < 3:
                print("Usage: tools/cli.py cat <remote_path>")
                sys.exit(1)
            print(conn.read_file(sys.argv[2]))

        elif cmd == "run":
            filepath = sys.argv[2] if len(sys.argv) > 2 else "src/main.py"
            conn.run_file(filepath)

        elif cmd == "deploy":
            filepath = sys.argv[2] if len(sys.argv) > 2 else "src/main.py"
            with open(filepath, "rb") as f:
                content = f.read()
            # Deploy to both apps/ and main.py
            conn.write_file("apps/sensor_dashboard.py", content)
            conn.write_file("main.py", content)
            print("Soft-resetting StickS3...")
            conn.enter_raw_repl()
            conn.ser.write(b"\x04")
            time.sleep(0.5)
            print("Done! Application deployed to :apps/sensor_dashboard.py and :main.py")

        elif cmd == "deploy-app":
            filepath = sys.argv[2] if len(sys.argv) > 2 else "src/main.py"
            with open(filepath, "rb") as f:
                content = f.read()
            conn.write_file("apps/sensor_dashboard.py", content)
            print("Done! Application installed into :apps/sensor_dashboard.py (available in APP LIST)")

        elif cmd == "set-boot-menu":
            code = b"""
import esp32
nvs = esp32.NVS('uiflow')
nvs.set_u8('boot_option', 1)
nvs.commit()
print('Boot option set to 1 (Startup Menu / APP LIST enabled on boot)')
"""
            print(conn.exec_raw(code))

        elif cmd == "set-boot-direct":
            code = b"""
import esp32
nvs = esp32.NVS('uiflow')
nvs.set_u8('boot_option', 0)
nvs.commit()
print('Boot option set to 0 (Direct boot into main.py enabled)')
"""
            print(conn.exec_raw(code))


        elif cmd == "repl":
            print(f"Connecting to REPL on {conn.port}...")
            conn.ser.write(b"\r\x03\r\x02")  # Exit raw REPL to friendly REPL
            conn.close()
            # Launch picocom, miniterm, or python repl
            os.system(f"python3 -m serial.tools.miniterm {conn.port} 115200")

        else:
            print(f"Unknown command: {cmd}")
            sys.exit(1)

    finally:
        conn.close()

if __name__ == "__main__":
    main()
