#!/usr/bin/env python3
"""Save the ESP32's Serial output to a file while you type commands.

    python analysis/capture.py --port COM5 data/raw/session1.csv      (Windows)
    python analysis/capture.py --port /dev/ttyUSB0 data/raw/session1.csv

Close the Arduino Serial Monitor first: only one program can use the port.
Everything the ESP32 prints is saved (CSV lines and '#' messages). Type a
command and press Enter to send it, e.g.  label E2_light_s1  or  mark inject_ldr.
Press Ctrl+C to stop.
"""
import argparse
import sys
import threading
from pathlib import Path

try:
    import serial
except ImportError:
    sys.exit("Install pyserial first:  pip install pyserial")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", help="file to write, e.g. data/raw/E2_s1.csv")
    ap.add_argument("--port", required=True, help="COM5, /dev/ttyUSB0, /dev/cu.usbserial-…")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    ser = serial.Serial(args.port, args.baud, timeout=1)

    def send_commands():
        for line in sys.stdin:
            ser.write((line.strip() + "\n").encode())

    threading.Thread(target=send_commands, daemon=True).start()
    rows = 0
    print(f"Saving to {out}. Type commands + Enter. Ctrl+C to stop.")
    with out.open("a", encoding="utf-8") as f:
        try:
            while True:
                line = ser.readline().decode(errors="replace").rstrip()
                if not line:
                    continue
                f.write(line + "\n")
                f.flush()
                if line.startswith("#"):
                    print(line)
                elif not line.startswith("ms,"):
                    rows += 1
                    if rows % 30 == 0:
                        cols = line.split(",")
                        print(f"  {rows} rows · label {cols[1]} · current {cols[12]} A · faults {cols[-1] or '-'}")
        except KeyboardInterrupt:
            print(f"\nStopped. {rows} rows saved to {out}")


if __name__ == "__main__":
    main()
