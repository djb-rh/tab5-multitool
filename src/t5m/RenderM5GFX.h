// Default drawing for the widgets, on M5GFX (what the Tab5's M5Unified
// uses). The widgets themselves never draw, so an LVGL app can skip this
// file and draw from Keypad::keyAt() / Trackpad::buttonRect() instead.
#pragma once

#include <M5GFX.h>

#include "Keypad.h"
#include "MacroPad.h"
#include "Trackpad.h"

namespace t5m {

struct Theme {
  uint32_t background = 0x101418;
  uint32_t key = 0x2C333B;
  uint32_t keyMod = 0x38414B;
  uint32_t keyLit = 0x2F7BD8;
  uint32_t text = 0xE8ECF0;
  uint32_t textDim = 0x8A95A0;
  uint32_t pad = 0x1A2027;
  uint32_t padEdge = 0x3A434D;
  int radius = 10;
};

class KeypadRenderer {
 public:
  // Draws the keys that changed since the last call (all of them after a
  // page change or the first time). Cheap to call every frame.
  void draw(lgfx::LovyanGFX &g, Keypad &kp, const Theme &t = Theme());
  void invalidate() { full_ = true; }

 private:
  void drawKey(lgfx::LovyanGFX &g, const KeyDef &d, const char *label, const Rect &r, bool lit, const Theme &t);
  bool full_ = true;
  int page_ = -1;
  bool lit_[96] = {};
  char label_[96][8] = {};  // what each key showed last time
};

class MacroPadRenderer {
 public:
  // Redraws the whole pad on a page switch, otherwise only what a press changed.
  void draw(lgfx::LovyanGFX &g, MacroPad &mp, const Theme &t = Theme());
  void invalidate() { full_ = true; }

 private:
  void drawButton(lgfx::LovyanGFX &g, const MacroPad &mp, int i, const Theme &t);
  void drawTab(lgfx::LovyanGFX &g, const MacroPad &mp, int i, const Theme &t);
  bool full_ = true;
  int page_ = -1, pressed_ = -1, pressedTab_ = -1;
};

class TrackpadRenderer {
 public:
  void draw(lgfx::LovyanGFX &g, const Trackpad &tp, const Theme &t = Theme());
  void invalidate() { full_ = true; }

 private:
  bool full_ = true;
  uint8_t buttons_ = 0xFF;
};

}  // namespace t5m
