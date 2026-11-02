// Touch points in screen coordinates, whatever reads the panel. Widgets take
// these so they work with M5.Touch, LVGL's input, or a test harness.
#pragma once

#include <cstdint>

namespace t5m {

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
  bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};

struct TouchPoint {
  int id = -1;  // stable while the finger stays down
  int x = 0, y = 0;
  bool down = false;
};

constexpr int kMaxTouches = 5;

// Fills `out` from M5.Touch (M5Unified); returns how many fingers are down.
int readM5Touch(TouchPoint out[kMaxTouches]);

}  // namespace t5m
