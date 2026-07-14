#!/usr/bin/env python3
"""Test suite for the ESP32USBHID Arduino library.

Requires python3-evdev (pip install evdev / apt install python3-evdev) for the
host-side HID event monitor.

Runs on a Linux host with an ESP32-S3 (USB-OTG / TinyUSB) connected:
  * UART bridge  -> /dev/ttyUSB0   (used for flashing + Serial)
  * native USB   -> enumerates as a composite HID device (keyboard +
                    mouse + consumer), exposed via /dev/input

Two checks are performed:
  1. Logic test  - on-device unit checks of keycode tables, reference
                    counting, 6-key rollover, and modifier/consumer state.
  2. Integration  - flash the auto-trigger example and confirm the host
                    actually receives the keyboard / mouse / consumer events.

Usage:
    python3 test/run_tests.py [--port /dev/ttyUSB0]
                             [--fqbn esp32:esp32:esp32s3:USBMode=default]

Exits non-zero if any check fails.
"""

import argparse
import os
import subprocess
import sys
import threading
import time

try:
    import evdev
    from evdev import ecodes
except ImportError:
    sys.exit("ERROR: python3-evdev is required. Install with:\n"
             "    apt install python3-evdev   (or: pip install evdev)")

# Linux keycodes / event types the demo firmware is expected to emit.
KEY_A = getattr(ecodes, "KEY_A")
KEY_VOLUMEUP = getattr(ecodes, "KEY_VOLUMEUP")
BTN_LEFT = getattr(ecodes, "BTN_LEFT")
BTN_RIGHT = getattr(ecodes, "BTN_RIGHT")
REL_WHEEL = getattr(ecodes, "REL_WHEEL")
EV_KEY = getattr(ecodes, "EV_KEY")
EV_REL = getattr(ecodes, "EV_REL")


def discover_devices():
    """Find the Espressif keyboard and mouse event devices via evdev.

    Returns {'kbd': path, 'mouse': path} or omit a key if not found yet.
    """
    found = {}
    for path in evdev.list_devices():
        try:
            dev = evdev.InputDevice(path)
        except OSError:
            continue
        name = dev.name
        caps = dev.capabilities().get(EV_KEY, [])
        dev.close()
        if "ESP32S3_DEV" not in name and "Espressif" not in name:
            continue
        if BTN_LEFT in caps:
            found.setdefault("mouse", path)
        else:
            found.setdefault("kbd", path)
    return found


def wait_for_enum(timeout=25):
    """Block until both Espressif keyboard and mouse nodes are present."""
    t0 = time.time()
    while time.time() - t0 < timeout:
        d = discover_devices()
        if "kbd" in d and "mouse" in d:
            return True
        time.sleep(0.3)
    return False


def run(cmd):
    print("+ " + " ".join(cmd))
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write(r.stdout + r.stderr)
        sys.exit("command failed: " + " ".join(cmd))
    return r


def monitor_events(paths, duration):
    def mon(path, label, out):
        dev = evdev.InputDevice(path)
        end = time.time() + duration
        while time.time() < end:
            try:
                ev = dev.read_one()
            except OSError:
                time.sleep(0.05)
                continue
            if ev is None:
                time.sleep(0.005)
                continue
            if ev.type == EV_KEY and ev.value == 1:
                out[label].setdefault("keys", set()).add(ev.code)
            elif ev.type == EV_REL and ev.code == REL_WHEEL and ev.value != 0:
                out[label].setdefault("wheel", set()).add(ev.code)
        dev.close()

    out = {label: {} for label in paths}
    threads = [threading.Thread(target=mon, args=(p, lbl, out))
               for lbl, p in paths.items()]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    return out


def test_logic(repo, port, fqbn, libs):
    print("\n=== Logic test: keycode tables, ref counting, state ===")
    sketch = os.path.join(repo, "test", "test_logic")
    run(["arduino-cli", "compile", "--fqbn", fqbn, "--libraries", libs,
         "-u", "-p", port, sketch])
    import serial
    # Single connection: pulse DTR to reboot, then read the same handle.
    # (Closing/reopening can leave RTS low on this board and force the
    #  ESP32-S3 into download mode, so we never close mid-test.)
    s = serial.Serial(port, 115200, timeout=1)
    s.dtr = False
    s.rts = False
    time.sleep(0.1)
    s.dtr = True
    time.sleep(0.1)
    s.dtr = False
    time.sleep(1)
    buf = ""
    end = time.time() + 7
    while time.time() < end:
        b = s.read(400)
        if b:
            buf += b.decode(errors="replace")
    s.close()
    print(buf.strip())
    fails = [l for l in buf.splitlines()
             if l.startswith("LOGIC") and l.endswith("FAIL")]
    ok = ("LOGIC ALL PASS" in buf) and not fails
    print("RESULT:", "PASS" if ok else "FAIL")
    return ok


def test_integration(repo, port, fqbn, libs):
    print("\n=== Integration test: HID events reach the host ===")
    example = os.path.join(repo, "examples", "BasicKeyboard")
    run(["arduino-cli", "compile", "--fqbn", fqbn, "--libraries", libs,
         "-u", "-p", port, example])
    if not wait_for_enum():
        print("ERROR: HID device did not enumerate")
        return False
    devices = discover_devices()
    kbd = devices.get("kbd")
    mouse = devices.get("mouse")
    print(f"monitoring {kbd} and {mouse}")
    seen = monitor_events({"kbd": kbd, "mouse": mouse}, 10)
    print("seen keycodes:", {k: sorted(v.get("keys", set()))
                             for k, v in seen.items()})
    print("seen scroll:", {k: sorted(v.get("wheel", set()))
                           for k, v in seen.items()})
    ok = (KEY_A in seen["kbd"].get("keys", set())
          and KEY_VOLUMEUP in seen["kbd"].get("keys", set())
          and BTN_LEFT in seen["mouse"].get("keys", set())
          and BTN_RIGHT in seen["mouse"].get("keys", set())
          and REL_WHEEL in seen["mouse"].get("wheel", set()))
    print("RESULT:", "PASS" if ok else "FAIL")
    return ok


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(here)          # .../ESP32USBHID
    libs = os.path.dirname(repo)          # parent dir containing the library
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyUSB0")
    ap.add_argument("--fqbn",
                    default="esp32:esp32:esp32s3:USBMode=default")
    args = ap.parse_args()

    # Logic first (no HID), integration last (ends with the demo firmware).
    results = [
        ("logic", test_logic(repo, args.port, args.fqbn, libs)),
        ("integration", test_integration(repo, args.port, args.fqbn, libs)),
    ]

    print("\n=== SUMMARY ===")
    for name, ok in results:
        print(f"  {name}: {'PASS' if ok else 'FAIL'}")
    sys.exit(0 if all(ok for _, ok in results) else 1)


if __name__ == "__main__":
    main()
