"""
Serial Capture and Stroke Dumper Tool for Digital Slate Calculator
Captures stroke dumps exported by ESP32 over USB-UART and saves them to /data.
"""

import sys
import time
import json
import argparse
from pathlib import Path
import serial

def find_serial_port(default_port="COM21"):
    return default_port

def dump_strokes(port="COM21", baud=115200, output_path=None, timeout=5.0):
    print(f"Connecting to {port} @ {baud}...")
    try:
        ser = serial.Serial(port, baud, timeout=1.0)
    except Exception as e:
        print(f"Error opening port {port}: {e}")
        return False

    time.sleep(1.5)  # Allow DTR reset settling if any
    ser.reset_input_buffer()

    print("Requesting stroke dump (sending 'd')...")
    ser.write(b'd\n')
    ser.flush()

    capturing = False
    json_lines = []
    start_time = time.time()

    while time.time() - start_time < timeout:
        line = ser.readline().decode('utf-8', errors='replace').strip()
        if not line:
            continue
        print(f"[ESP32] {line}")

        if "--- BEGIN STROKE DUMP ---" in line:
            capturing = True
            json_lines = []
            continue
        elif "--- END STROKE DUMP ---" in line:
            capturing = False
            break
        elif capturing:
            json_lines.append(line)

    ser.close()

    if json_lines:
        json_str = "\n".join(json_lines).strip()
        try:
            parsed = json.loads(json_str)
            total_pts = parsed.get('total_points', parsed.get('point_count', 0))
            print(f"\n[SUCCESS] Captured valid JSON! Stroke count: {parsed.get('stroke_count', 0)}, Total points: {total_pts}")
            
            if output_path is None:
                timestamp = int(time.time())
                output_path = Path(__file__).resolve().parent.parent / "data" / f"strokes_{timestamp}.json"
            else:
                output_path = Path(output_path)
            
            output_path.parent.mkdir(parents=True, exist_ok=True)
            with open(output_path, "w", encoding="utf-8") as f:
                json.dump(parsed, f, indent=2)
            print(f"Saved stroke dump to: {output_path}")
            return True
        except json.JSONDecodeError as err:
            print(f"[ERROR] Failed to parse captured JSON: {err}")
            print("Raw text:")
            print(json_str)
            return False
    else:
        print("[WARNING] No stroke dump marker captured within timeout.")
        return False

def interactive_monitor(port="COM21", baud=115200):
    print(f"Starting monitor on {port} @ {baud}. Press Ctrl+C to exit.")
    print("Commands: 'c' = Clear, 'u' = Undo, 'd' = Dump JSON")
    try:
        ser = serial.Serial(port, baud, timeout=0.1)
    except Exception as e:
        print(f"Error opening port {port}: {e}")
        return

    time.sleep(1.0)
    try:
        while True:
            line = ser.readline().decode('utf-8', errors='replace').rstrip()
            if line:
                print(f"[ESP32] {line}")
    except KeyboardInterrupt:
        print("\nExiting monitor.")
    finally:
        ser.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Digital Slate Serial Tool")
    parser.add_argument("--port", default="COM21", help="Serial COM port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    parser.add_argument("--dump", action="store_true", help="Dump strokes to JSON and exit")
    parser.add_argument("--out", default=None, help="Output JSON path")

    args = parser.parse_args()

    if args.dump:
        success = dump_strokes(port=args.port, baud=args.baud, output_path=args.out)
        sys.exit(0 if success else 1)
    else:
        interactive_monitor(port=args.port, baud=args.baud)
