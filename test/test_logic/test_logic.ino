/* On-device logic tests for ESP32USBHID.
   Exercises the pure logic (keycode tables, reference counting, rollover,
   modifier/consumer state) WITHOUT calling begin(), so no HID reports are
   sent to the host. Results are printed to Serial as:
       LOGIC <name> PASS
       LOGIC <name> FAIL
       LOGIC ALL PASS
   The host harness (../run_tests.py) parses these lines.
*/

#include <ESP32USBHID.h>

ESP32USBHID HID;

static int g_fail = 0;

#define CHECK(name, cond)                                         \
  do {                                                            \
    if (cond) Serial.println("LOGIC " name " PASS");             \
    else { Serial.println("LOGIC " name " FAIL"); g_fail++; }     \
  } while (0)

void setup() {
  Serial.begin(115200);
  delay(500);

  // ---------------- charToHID ----------------
  CHECK("char_a",        HID.charToHID('a') == 0x04);
  CHECK("char_A",        HID.charToHID('A') == 0x04);
  CHECK("char_1",        HID.charToHID('1') == 0x1E);
  CHECK("char_Z",        HID.charToHID('Z') == 0x1D);
  CHECK("char_space",    HID.charToHID(' ') == KEY_SPACE);
  CHECK("char_unmapped", HID.charToHID(0x01) == 0);

  // ---------------- needsShift ----------------
  CHECK("shift_upper",   HID.needsShift('A') == true);
  CHECK("shift_lower",   HID.needsShift('a') == false);
  CHECK("shift_digit",   HID.needsShift('1') == false);
  CHECK("shift_symbol",  HID.needsShift('!') == true);

  // ---------------- keyboard ref counting ----------------
  HID.releaseAllKeys();
  HID.pressKey(0x04);
  HID.pressKey(0x04);                       // ref-counted: still 1 pressed
  CHECK("kbd_refcount",  HID.getPressedCount() == 1);
  HID.releaseKey(0x04);                     // still held (ref 1)
  CHECK("kbd_refheld",   HID.getPressedCount() == 1);
  HID.releaseKey(0x04);                     // now fully released
  CHECK("kbd_released",  HID.getPressedCount() == 0);

  // 6-key rollover cap
  for (int i = 0; i < 8; i++) HID.pressKey(0x04 + i);
  CHECK("kbd_rollover",  HID.getPressedCount() <= 6);
  HID.releaseAllKeys();
  CHECK("kbd_cleared",   HID.getPressedCount() == 0);

  // ---------------- modifiers ----------------
  HID.setModifierState(0);
  HID.pressModifier(MOD_LSHIFT);
  CHECK("mod_press",     HID.getModifierState() == MOD_LSHIFT);
  HID.releaseModifier(MOD_LSHIFT);
  CHECK("mod_release",   HID.getModifierState() == 0);
  HID.setModifierState(MOD_LCTRL | MOD_LALT);
  CHECK("mod_set",       HID.getModifierState() == (MOD_LCTRL | MOD_LALT));
  HID.setModifierState(0);

  // ---------------- consumer ----------------
  HID.pressConsumer(MEDIA_PLAY);
  CHECK("cons_press",    HID.getConsumerState() == MEDIA_PLAY);
  HID.releaseConsumer();
  CHECK("cons_release",  HID.getConsumerState() == 0);

  // ---------------- mouse scroll (5-byte report) ----------------
  // scrollMouse must not clobber the held button state, and the new API
  // must not crash before begin() (SendReport gracefully returns false).
  HID.setMouseButtons(MOUSE_BTN_LEFT);
  HID.scrollMouse(5, 2);
  CHECK("scroll_preserves_btns", HID.getMouseButtons() == MOUSE_BTN_LEFT);
  HID.setMouseButtons(0);
  HID.scrollMouse(0, 0);
  CHECK("scroll_zero_btns",      HID.getMouseButtons() == 0);

  Serial.println(g_fail == 0 ? "LOGIC ALL PASS" : "LOGIC SOME FAIL");
}

void loop() {}
