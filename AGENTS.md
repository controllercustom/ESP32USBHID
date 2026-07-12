# AGENTS.md — ESP32USBHID

Arduino library (not a sketch): `library.properties` declares `architectures=esp32`.
It wraps the ESP32 Arduino core's USB-OTG / TinyUSB `USBHIDDevice` as a single
composite HID device (Keyboard report ID 1 + Mouse ID 2 + Consumer ID 3).

## Verification

Two ways to verify, both need an ESP32-S3 connected over UART (`/dev/ttyUSB0`)
and its native USB port plugged in so it enumerates as the HID device:

**1. Full suite (`test/run_tests.py`)** — runs on-device logic checks plus a
host-side check that the keyboard/mouse/consumer events actually reach Linux
`/dev/input`. Ends with the demo firmware flashed and enumerating as HID.
Requires `python3-evdev` (`apt install python3-evdev`) for the event monitor;
device discovery is capability-based via `evdev`, not by hardcoded node names.

```bash
python3 test/run_tests.py            # auto-detects port/event nodes
python3 test/run_tests.py --port /dev/ttyUSB0
```

**2. Just compile the example** (no hardware required to build):

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:USBMode=default" \
  --libraries .. examples/BasicKeyboard
```

`--libraries` must point at a directory that **contains** this repo folder
(here `..` = `/home/pi`), so `ESP32USBHID` is found as a library. Do not point
it at the repo root itself. Requires the `esp32:esp32` core (3.3.8 installed).

`examples/BasicKeyboard` is an **auto-trigger demo** (fires a key/mouse/media
sequence every 2 s) — it does NOT use the BOOT button, so it and the test
suite run fully unattended.

## Build / runtime quirks

- **FQBN must use USB-OTG (TinyUSB) mode**: `USBMode=default`. The default
  `hwcdc` is Hardware CDC+JTAG and will not expose this HID device, even though
  it still compiles. ESP32-S2/S3 both have the USB-OTG peripheral this needs.
- Target hardware in the example is an **ESP32-S3 Dev Module** (ESP32-S2/S3
  both have the USB-OTG peripheral). The example is **auto-trigger** and needs
  no button; if you wire one, `GPIO 0` is the BOOT button (`INPUT_PULLUP`,
  active LOW).
- Upload over USB-OTG CDC uses `UploadMode=cdc`; otherwise use UART
  (`/dev/ttyACM0` etc.). `arduino-cli upload -p <port> --fqbn …` (or
  `compile -u`) flashes over that transport; `arduino-cli upload -p <ip> …
  --upload-field password=…` does a **native OTA** upload to a board running an
  OTA service (e.g. ArduinoOTA) — but the bundled example does **not** enable
  OTA, so use serial upload for it. Serial Monitor talks to the app's `Serial`
  (UART), separate from the HID USB link.

## Architecture notes (non-obvious)

- The constructor has a `static bool once` guard, so `hid.addDevice(...)` runs
  only for the first instance. **Treat `ESP32USBHID` as a single instance** —
  creating a second object will not register a second device and its reports
  go nowhere.
- `pressKey`/`releaseKey` use reference counting (`keyRefCount[256]`) and 6-key
  rollover; `releaseConsumer()` takes no argument and clears the single 16-bit
  usage (consumer control has no true key-up/down distinction).
- `pressModifier`/`releaseModifier` are also reference-counted, per bit
  (`modRefCount[8]`): a modifier bit is only cleared once its release count
  matches its press count. All three counters reset in `releaseAllKeys()` /
  `releaseAll()`.
- `charToHID` / `needsShift` tables are `PROGMEM` — harmless on ESP32, kept for
  AVR parity; don't "optimize" them away expecting a behavior change.
- `setModifierState()` intentionally does **not** send a report; caller must
  trigger a send (press/release a key or call `sendKeyboardReport` indirectly).

## Conventions

- `keywords.txt` is Arduino-IDE syntax coloring only; it is not used at compile
  time and does not need to stay in sync with the API for builds to pass.
- Public API surface (methods/constants) is the source of truth for
  `keywords.txt`; update both when adding symbols.
