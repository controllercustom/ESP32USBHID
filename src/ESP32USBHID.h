// SPDX-License-Identifier: MIT
// See LICENSE for the full license text.

#ifndef ESP32USBHID_H
#define ESP32USBHID_H

// Library version (keep in sync with library.properties).
#define ESP32USBHID_VERSION "0.1.0"

#include <Arduino.h>
#include <USB.h>
#include <USBHID.h>

// Modifier key masks
#define MOD_LCTRL  0x01
#define MOD_LSHIFT 0x02
#define MOD_LALT   0x04
#define MOD_LGUI   0x08

// Mouse button masks (for HID report byte 0)
#define MOUSE_BTN_LEFT   0x01
#define MOUSE_BTN_RIGHT  0x02
#define MOUSE_BTN_MIDDLE 0x04

// Keyboard keycodes
#define KEY_UP         0x52
#define KEY_DOWN       0x51
#define KEY_LEFT       0x50
#define KEY_RIGHT      0x4F
#define KEY_DELETE     0x4C
#define KEY_HOME       0x4A
#define KEY_END        0x4D
#define KEY_PAGE_UP    0x49
#define KEY_PAGE_DOWN  0x4B
#define KEY_INSERT     0x4E
#define KEY_TAB        0x2B
#define KEY_CAPS_LOCK  0x39
#define KEY_NUM_LOCK   0x53
#define KEY_SPACE      0x2C
#define KEY_F1         0x3A
#define KEY_NP0        0x62
#define KEY_NP1        0x59
#define KEY_NP2        0x5A
#define KEY_NP3        0x5B
#define KEY_NP4        0x5C
#define KEY_NP5        0x5D
#define KEY_NP6        0x5E
#define KEY_NP7        0x5F
#define KEY_NP8        0x60
#define KEY_NP9        0x61
#define KEY_NP_ADD     0x57
#define KEY_NP_SUB     0x56
#define KEY_NP_MUL     0x55
#define KEY_NP_DIV     0x54
#define KEY_NP_ENT     0x58
#define KEY_NP_DOT     0x63

// Consumer (media) usages (16-bit)
#define MEDIA_VOL_UP   0x00E9
#define MEDIA_VOL_DN   0x00EA
#define MEDIA_MUTE     0x00E2
#define MEDIA_PLAY     0x00CD
#define MEDIA_NEXT     0x00B5
#define MEDIA_PREV     0x00B6
#define MEDIA_FF       0x00B3
#define MEDIA_RW       0x00B4

class ESP32USBHID : public USBHIDDevice {
public:
  ESP32USBHID();

  void begin();

  // ---- Keyboard ----

  // Raw 8-byte report: [modifiers, reserved, key0..key5]
  bool sendKeyboard(const uint8_t data[8]);

  // High-level key press/release with reference counting and 6-key rollover
  void pressKey(uint8_t kc);
  void releaseKey(uint8_t kc);

  // Release all pressed keyboard keys and modifiers
  void releaseAllKeys();

  // Modifier management (immediately updates the report)
  void pressModifier(uint8_t mod);
  void releaseModifier(uint8_t mod);
  uint8_t getModifierState() const;
  void setModifierState(uint8_t state);
  int getPressedCount() const;

  // ---- Mouse ----

  // Raw 4-byte report: [buttons, x, y, wheel]
  bool sendMouse(const uint8_t data[4]);

  // Movement sends current buttons + passed delta
  void moveMouse(int8_t dx, int8_t dy);

  // Button access (immediately updates the report)
  void setMouseButtons(uint8_t btns);
  uint8_t getMouseButtons() const;

  // Convenience: press then release a button
  void clickMouse(uint8_t button);

  // ---- Consumer Control ----

  // Raw 2-byte report: [usage-low, usage-high]
  bool sendConsumer(const uint8_t data[2]);

  void pressConsumer(uint16_t usage);
  void releaseConsumer();
  uint16_t getConsumerState() const;

  // ---- LED / Output report ----

  // Host LED state (Num Lock, Caps Lock, Scroll Lock — bits 0/1/2)
  uint8_t getLEDState() const;

  // Register a callback invoked when the host sends an output report
  void onLED(void (*cb)(uint8_t));

  // ---- Global reset ----

  // Sends zero reports for keyboard, mouse, consumer and clears internal state
  void releaseAll();

  // ---- Keycode lookup tables ----

  static uint8_t charToHID(char c);
  static bool needsShift(char c);

  // ---- Version ----
  static const char* version() { return ESP32USBHID_VERSION; }

  // ---- USBHIDDevice overrides ----
  uint16_t _onGetDescriptor(uint8_t *dst) override;
  void _onOutput(uint8_t report_id, const uint8_t *buffer, uint16_t len) override;

private:
  USBHID hid;
  uint8_t modifierState;
  uint8_t pressedKeys[6];
  int pressedCount;
  uint8_t keyRefCount[256];
  uint8_t mouseBtns;
  uint16_t consumerUsage;
  uint8_t ledStateValue;
  void (*ledCallback)(uint8_t);

  void sendKeyboardReport();
};

#endif
