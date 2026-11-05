// M5Stack's Tab5 Keyboard (A164), the 70-key clip-on for the 2x5 header.
//
// An STM32 on its own I2C pins (SDA G0, SCL G1, INT G50, address 0x6D) scans
// the 5x14 matrix. This reads it in raw key mode - row, column, down/up - so
// every key reports its real state (held keys repeat on the host, Shift-click
// works) and the legends are mapped here, matching what is printed on the keys:
//
//   Sym  held: the second legend on a key (the one Sym gives on M5's firmware),
//        and an Fn layer on keys that have none (see Tab5Keyboard.cpp)
//   Aa   held: Shift. Double-tap: Caps Lock.
//   Ctrl, Alt: themselves. Sym+Alt: GUI (Cmd / Windows key).
//
// The Grove port's I2C bus is moved to the keyboard pins (the P4 has no third
// I2C port), so nothing else may use M5.Ex_I2C. Presence is probed while
// absent, so clipping the keyboard on mid-session is noticed.
#pragma once

#include <cstdint>

#include "hid_usage.h"

namespace t5m {

class Tab5Keyboard {
 public:
  // Call once after M5.begin(); safe without a keyboard attached.
  void begin();
  // Call often from loop(); reads queued key events while the INT line says
  // there are some, and probes for the keyboard while it is absent.
  void tick();
  bool present() const { return present_ && !pretendAbsent_; }
  // Testing: apps act as if it were unclipped (keys still arrive).
  void pretendAbsent(bool absent) { pretendAbsent_ = absent; }
  uint8_t firmware() const { return firmware_; }

  // The next key event, if any. Sym never produces one (it is a layer key).
  bool next(KeyEvent *out);
  // Drops the events waiting for next().
  void flush() { tail_ = head_; }
  // Counts every key event so far: a change means a key moved.
  uint32_t eventCount() const { return events_; }
  // Local key events the Sym layer reserves for the app (not for the host):
  // Sym+Esc. They arrive through next() with usage = kAppKey.
  static constexpr uint8_t kAppKey = 0xF0;

  // The keyboard's two status LEDs, 0..255 per channel before brightness.
  // brightness is 0..100 (the keyboard scales by it). Default 3: the LEDs
  // are very bright at full power.
  void setLed(int index, uint8_t r, uint8_t g, uint8_t b);
  void setLedBrightness(uint8_t percent);
  uint8_t ledBrightness() const { return led_brightness_; }

  // Which physical key is held, by matrix position (for diagnostics).
  bool held(int row, int col) const { return (held_[row] >> col) & 1; }

 private:
  bool bus();
  bool answers();
  void found();
  void readEvents();
  void handle(int row, int col, bool down);
  void push(const KeyEvent &e);
  void applyLeds();

  bool bus_ = false, present_ = false, pretendAbsent_ = false;
  uint8_t firmware_ = 0;
  uint32_t next_probe_ = 0, next_check_ = 0, next_poll_ = 0;
  uint16_t held_[5] = {};
  // What each held key sent, so its release matches even if Sym changed.
  uint8_t sent_usage_[5][14] = {};
  bool sym_ = false, aa_ = false, ctrl_ = false, alt_ = false;
  uint32_t aa_last_tap_ = 0, aa_down_at_ = 0;
  bool aa_other_ = false;  // another key went down while Aa was held
  static constexpr int kQueue = 32;
  KeyEvent queue_[kQueue];
  int head_ = 0, tail_ = 0;
  uint32_t events_ = 0;
  uint8_t leds_[2][3] = {};
  uint8_t led_brightness_ = 3;
  bool leds_dirty_ = true;
};

// What a key event types, for local text entry (US layout): a printable
// character, or 0 with `special` naming Enter/Escape/arrows/...
struct KeyText {
  char ch = 0;
  uint8_t special = 0;  // the usage of a non-printing key, else 0
  bool ctrl = false, alt = false;
  // False for releases, bare modifiers and keys that type nothing.
  static bool from(const KeyEvent &e, KeyText *out);
};

}  // namespace t5m
