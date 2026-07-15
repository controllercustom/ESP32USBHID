# ESP32USBHID

ESP32-S3 / ESP32-S2 USB HID library — a single composite device exposing a
**Keyboard** (report ID 1), **Mouse** (report ID 2), and **Consumer Control**
(media keys, report ID 3) over the chip's native USB-OTG (TinyUSB) port.

It wraps the ESP32 Arduino core's `USBHIDDevice` so you can send keystrokes,
mouse movement/clicks, and media keys with a few high-level calls.

## Requirements

- Board with USB-OTG: **ESP32-S3** or **ESP32-S2** (e.g. ESP32-S3 Dev Module).
- `esp32:esp32` Arduino core **3.x** (developed/tested with 3.3.8).
- The sketch **must be built with USB-OTG (TinyUSB) mode**: `USBMode=default`.
  The default `Hardware CDC and JTAG` mode will compile but will NOT expose
  this HID device.

## Install

Copy this folder into your Arduino `libraries/` directory (or use the
Arduino Library Manager if published). The library is picked up automatically
by any sketch that does `#include <ESP32USBHID.h>`.

## Using the Arduino IDE

1. **Install board support** — Arduino > Preferences > Additional Boards
   Manager URLs, add
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`, then
   install **esp32** via Boards Manager.
2. **Select the board** — Tools > Board > ESP32 Arduino > **ESP32S3 Dev Module**
   (or **ESP32S2 Dev Module**).
3. **Set USB mode** — Tools > USB Mode > **USB-OTG (TinyUSB)**. This is
   required: the default *Hardware CDC and JTAG* mode compiles but will NOT
   expose this HID device.
4. **Select the port** — Tools > Port > the board's UART port
   (e.g. `/dev/ttyUSB0` on Linux, `COMx` on Windows).
5. **Open the example** — File > Examples > **ESP32USBHID** > BasicKeyboard.
6. **Upload** — Sketch > Upload (Ctrl/Cmd+U).

After upload the board reboots and enumerates as a composite HID device. The
BasicKeyboard example auto-fires a key / mouse / media sequence every 2 s, so
you can verify it from the host without pressing any button.

## Quick start

```cpp
#include <ESP32USBHID.h>

ESP32USBHID HID;

void setup() {
  HID.begin();
}

void loop() {
  HID.pressKey(0x04);          // 'a'
  delay(10);
  HID.releaseKey(0x04);

  HID.clickMouse(MOUSE_BTN_LEFT);

  HID.pressConsumer(MEDIA_VOL_UP);
  delay(10);
  HID.releaseConsumer();

  delay(2000);
}
```

See `examples/BasicKeyboard` for a complete, auto-triggering demo (it fires a
key / mouse / media sequence every 2 s, with no button required).

## API

```cpp
begin()                         // start USB + register the HID device

// Keyboard
sendKeyboard(const uint8_t[8])  // raw 8-byte report
pressKey(uint8_t kc)            // press (reference-counted, 6-key rollover)
releaseKey(uint8_t kc)          // release
releaseAllKeys()
pressModifier(uint8_t mod)      // MOD_LCTRL / MOD_LSHIFT / MOD_LALT / MOD_LGUI (reference-counted, per bit)
releaseModifier(uint8_t mod)
setModifierState(uint8_t state) // sets state but does NOT send a report
getModifierState() / getPressedCount()

// Mouse
sendMouse(const uint8_t[5])     // raw 5-byte report [buttons, x, y, wheel, pan]
moveMouse(int8_t dx, int8_t dy)
scrollMouse(int8_t wheel, int8_t pan = 0) // vertical wheel + horizontal pan
setMouseButtons(uint8_t btns)   // MOUSE_BTN_LEFT / _RIGHT / _MIDDLE
getMouseButtons()
clickMouse(uint8_t button)

// Consumer (media)
sendConsumer(const uint8_t[2])  // raw 2-byte usage
pressConsumer(uint16_t usage)   // e.g. MEDIA_VOL_UP, MEDIA_PLAY, MEDIA_MUTE
releaseConsumer()               // clears the single 16-bit usage
getConsumerState()

// Host LEDs / output report
getLEDState()                   // Num/Caps/Scroll Lock bits (0/1/2)
onLED(void (*cb)(uint8_t))      // callback on host LED changes

releaseAll()                    // zero all reports + clear internal state

charToHID(char c) / needsShift(char c)   // ASCII -> HID keycode helpers

version()                       // static; returns ESP32USBHID_VERSION (e.g. "0.1.0")
```

Key constants are listed in `keywords.txt` and defined in `src/ESP32USBHID.h`
(`MOD_*`, `MOUSE_BTN_*`, `KEY_*`, `MEDIA_*`). The library version is defined by
the `ESP32USBHID_VERSION` macro and returned by the static `ESP32USBHID::version()`.

## Notes

- Treat `ESP32USBHID` as a **single instance** — the constructor registers the
  USB device only once, so a second object sends nothing.
- `setModifierState()` intentionally does not send a report; trigger a send by
  pressing/releasing a key or calling `releaseAll()`.

## Testing

`test/run_tests.py` runs on a Linux host with the board connected over UART
(`/dev/ttyUSB0`) and its native USB plugged in (so it enumerates as HID):

```bash
apt install python3-evdev
python3 test/run_tests.py
```

It (1) flashes an on-device logic test of the keycode tables, reference
counting, rollover, and modifier/consumer state, and (2) flashes the example
and verifies the keyboard / mouse / consumer events actually reach the host's
`/dev/input`. Ends with the demo firmware running.
