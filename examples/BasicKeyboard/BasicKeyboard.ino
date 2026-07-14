// SPDX-License-Identifier: MIT
// See LICENSE for the full license text.

/* ESP32USBHID demo — auto-trigger (no button required)
   Periodically sends a keystroke, a mouse click, and a media key so the
   device can be exercised without any physical input. This makes the demo
   and the host-side test suite (test/run_tests.py) fully automated.
*/

#include <ESP32USBHID.h>

ESP32USBHID HID;

// How often to fire the demo sequence, in milliseconds.
#define INTERVAL_MS 2000

void setup() {
  Serial.begin(115200);
  HID.begin();
  Serial.println("ESP32USBHID ready — auto-firing every 2s");
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last >= INTERVAL_MS) {
    last = millis();
    Serial.println("Sending keystroke, mouse click, media key");

    // Press/release the 'A' key
    HID.pressKey(0x04);   // HID usage for 'a'
    delay(10);
    HID.releaseKey(0x04);

    delay(100);

    // Left mouse button click
    HID.clickMouse(MOUSE_BTN_LEFT);

    delay(100);

    // Hold two mouse buttons down together to exercise the button bitmask
    HID.setMouseButtons(MOUSE_BTN_LEFT | MOUSE_BTN_RIGHT);
    delay(100);
    HID.setMouseButtons(0);

    delay(100);

    // Scroll the wheel up (vertical) — exercises the pan/scroll report bytes
    HID.scrollMouse(1);

    delay(100);

    // Volume up media key
    HID.pressConsumer(MEDIA_VOL_UP);
    delay(10);
    HID.releaseConsumer();
  }
}
