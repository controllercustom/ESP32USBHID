// SPDX-License-Identifier: MIT
// See LICENSE for the full license text.

#include "ESP32USBHID.h"
#include <cstring>

// ====================================================================
// HID Report Descriptor — Keyboard (ID1) + Mouse (ID2) + Consumer (ID3)
// ====================================================================
static const uint8_t hid_report_desc[] = {
  TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1)),
  TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(2)),
  TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(3))
};

// ====================================================================
// charToHID — ASCII to HID keycode, PROGMEM (indices 0..127)
// ====================================================================
uint8_t ESP32USBHID::charToHID(char c) {
  if ((uint8_t)c > 127) return 0;
  static const uint8_t PROGMEM table[128] = {
    0,0,0,0,0,0,0,0, 0x2A,0x2B,0x28,0,0,0x28,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0x29,0,0,0,0,
    0x2C,0x1E,0x34,0x20, 0x21,0x22,0x24,0x34,
    0x26,0x27,0x25,0x2E, 0x36,0x2D,0x37,0x38,
    0x27,0x1E,0x1F,0x20, 0x21,0x22,0x23,0x24,
    0x25,0x26,0x33,0x33, 0x36,0x2E,0x37,0x38,
    0x1F,0x04,0x05,0x06, 0x07,0x08,0x09,0x0A,
    0x0B,0x0C,0x0D,0x0E, 0x0F,0x10,0x11,0x12,
    0x13,0x14,0x15,0x16, 0x17,0x18,0x19,0x1A,
    0x1B,0x1C,0x1D,0x2F, 0x31,0x30,0x23,0x2D,
    0x35,0x04,0x05,0x06, 0x07,0x08,0x09,0x0A,
    0x0B,0x0C,0x0D,0x0E, 0x0F,0x10,0x11,0x12,
    0x13,0x14,0x15,0x16, 0x17,0x18,0x19,0x1A,
    0x1B,0x1C,0x1D,0x2F, 0x31,0x30,0x35,0
  };
  return pgm_read_byte(&table[(uint8_t)c]);
}

// ====================================================================
// needsShift — returns true if a printable ASCII character requires
//              the Shift modifier (PROGMEM, indices 0..127)
// ====================================================================
bool ESP32USBHID::needsShift(char c) {
  static const bool PROGMEM table[128] = {
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,1,1,1,1,1,1,0, 1,1,1,1,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,1,0,1,0,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,0,0,0,1,1,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,1,1,1,1,0
  };
  return ((uint8_t)c) < 128 && pgm_read_byte(&table[(uint8_t)c]);
}

// ====================================================================
// Constructor — registers device via USBHID (once guard)
// ====================================================================
ESP32USBHID::ESP32USBHID()
  : modifierState(0), pressedCount(0), mouseBtns(0),
    consumerUsage(0), ledStateValue(0), ledCallback(nullptr) {
  memset(pressedKeys, 0, sizeof(pressedKeys));
  memset(keyRefCount, 0, sizeof(keyRefCount));
  static bool once = false;
  if (!once) {
    once = true;
    hid.addDevice(this, sizeof(hid_report_desc));
  }
}

void ESP32USBHID::begin() {
  USB.usbClass(0);
  USB.usbSubClass(0);
  USB.usbProtocol(0);
  USB.begin();
  hid.begin();
}

// ====================================================================
// USBHIDDevice overrides
// ====================================================================
uint16_t ESP32USBHID::_onGetDescriptor(uint8_t *dst) {
  memcpy(dst, hid_report_desc, sizeof(hid_report_desc));
  return sizeof(hid_report_desc);
}

void ESP32USBHID::_onOutput(uint8_t report_id, const uint8_t *buffer, uint16_t len) {
  if (report_id == 1 && len >= 1) {
    ledStateValue = buffer[0];
    if (ledCallback) ledCallback(ledStateValue);
  }
}

// ====================================================================
// Keyboard
// ====================================================================
bool ESP32USBHID::sendKeyboard(const uint8_t data[8]) {
  return hid.SendReport(1, data, 8);
}

void ESP32USBHID::sendKeyboardReport() {
  uint8_t report[8] = { modifierState, 0 };
  for (int i = 0; i < pressedCount && i < 6; i++) {
    report[2 + i] = pressedKeys[i];
  }
  sendKeyboard(report);
}

void ESP32USBHID::pressKey(uint8_t kc) {
  if (keyRefCount[kc] == 0) {
    for (int i = 0; i < pressedCount; i++) {
      if (pressedKeys[i] == kc) return;
    }
    if (pressedCount < 6) {
      pressedKeys[pressedCount++] = kc;
    }
  }
  if (keyRefCount[kc] < 255) keyRefCount[kc]++;
  sendKeyboardReport();
}

void ESP32USBHID::releaseKey(uint8_t kc) {
  if (keyRefCount[kc] > 0) keyRefCount[kc]--;
  if (keyRefCount[kc] > 0) return;
  for (int i = 0; i < pressedCount; i++) {
    if (pressedKeys[i] == kc) {
      pressedKeys[i] = pressedKeys[--pressedCount];
      pressedKeys[pressedCount] = 0;
      break;
    }
  }
  sendKeyboardReport();
}

void ESP32USBHID::releaseAllKeys() {
  pressedCount = 0;
  memset(pressedKeys, 0, sizeof(pressedKeys));
  memset(keyRefCount, 0, sizeof(keyRefCount));
  modifierState = 0;
  sendKeyboardReport();
}

void ESP32USBHID::pressModifier(uint8_t mod) {
  modifierState |= mod;
  sendKeyboardReport();
}

void ESP32USBHID::releaseModifier(uint8_t mod) {
  modifierState &= ~mod;
  sendKeyboardReport();
}

uint8_t ESP32USBHID::getModifierState() const {
  return modifierState;
}

void ESP32USBHID::setModifierState(uint8_t state) {
  modifierState = state;
  // Note: does NOT send — caller is expected to press/release a key which
  // triggers sendKeyboardReport, or call releaseAll/sendKeyboardReport directly.
}

int ESP32USBHID::getPressedCount() const {
  return pressedCount;
}

// ====================================================================
// Mouse
// ====================================================================
bool ESP32USBHID::sendMouse(const uint8_t data[4]) {
  return hid.SendReport(2, data, 4);
}

void ESP32USBHID::moveMouse(int8_t dx, int8_t dy) {
  uint8_t report[4] = { mouseBtns, (uint8_t)dx, (uint8_t)dy, 0 };
  sendMouse(report);
}

void ESP32USBHID::setMouseButtons(uint8_t btns) {
  mouseBtns = btns;
  moveMouse(0, 0);
}

uint8_t ESP32USBHID::getMouseButtons() const {
  return mouseBtns;
}

void ESP32USBHID::clickMouse(uint8_t button) {
  uint8_t prev = mouseBtns;
  mouseBtns = button;
  moveMouse(0, 0);
  mouseBtns = prev;
  moveMouse(0, 0);
}

// ====================================================================
// Consumer Control
// ====================================================================
bool ESP32USBHID::sendConsumer(const uint8_t data[2]) {
  return hid.SendReport(3, data, 2);
}

void ESP32USBHID::pressConsumer(uint16_t usage) {
  consumerUsage = usage;
  uint8_t r[2] = { (uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8) };
  sendConsumer(r);
}

void ESP32USBHID::releaseConsumer() {
  consumerUsage = 0;
  uint8_t r[2] = { 0, 0 };
  sendConsumer(r);
}

uint16_t ESP32USBHID::getConsumerState() const {
  return consumerUsage;
}

// ====================================================================
// LED
// ====================================================================
uint8_t ESP32USBHID::getLEDState() const {
  return ledStateValue;
}

void ESP32USBHID::onLED(void (*cb)(uint8_t)) {
  ledCallback = cb;
}

// ====================================================================
// Global reset
// ====================================================================
void ESP32USBHID::releaseAll() {
  pressedCount = 0;
  memset(pressedKeys, 0, sizeof(pressedKeys));
  memset(keyRefCount, 0, sizeof(keyRefCount));
  modifierState = 0;
  mouseBtns = 0;
  consumerUsage = 0;
  uint8_t zero[8] = { 0 };
  sendKeyboard(zero);
  uint8_t mzero[4] = { 0 };
  sendMouse(mzero);
  uint8_t czero[2] = { 0 };
  sendConsumer(czero);
}
