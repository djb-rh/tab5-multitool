// A touchpad in a rectangle of the screen, driving a HidSink's mouse.
//
//   one finger moves the pointer (with acceleration)
//   tap = left click, two-finger tap = right click, three-finger tap = middle
//   two fingers dragging = scroll (vertical and horizontal)
//   tap then press again and drag = drag with the left button held (a tap
//   clicks after a short wait, so it can become a drag; two taps = double-click)
//   optional button strip along the bottom edge: left | middle | right,
//   held for as long as the finger is on them (so drags work there too)
#pragma once

#include <cstdint>

#include "HidSink.h"
#include "Touch.h"

namespace t5m {

struct TrackpadConfig {
  float speed = 1.6f;         // pointer counts per pixel at slow speed
  float accel = 0.06f;        // extra gain per px/frame of finger speed
  float scrollPx = 28.0f;     // finger pixels per wheel click
  bool naturalScroll = true;  // content follows the fingers (macOS default)
  bool tapToClick = true;
  int buttonStrip = 90;       // height of the L/M/R strip, 0 = none
};

class Trackpad {
 public:
  explicit Trackpad(HidSink &sink) : sink_(sink) {}
  void setRect(const Rect &r) { rect_ = r; }
  const Rect &rect() const { return rect_; }
  TrackpadConfig &config() { return cfg_; }

  // Feed every touch frame. Fingers that start outside the rectangle are ignored.
  void handle(const TouchPoint *touches, int n, uint32_t now_ms);

  // The strip's button rectangles (for drawing); false if there is no strip.
  bool buttonRect(int i, Rect *r) const;
  uint8_t stripButtons() const { return stripHeld_; }
  bool padRect(Rect *r) const;

 private:
  struct Finger {
    int id = -1;
    int x = 0, y = 0, x0 = 0, y0 = 0;
    uint8_t stripButton = 0;  // nonzero: this finger is on a strip button
  };
  void endGesture(uint32_t now);

  HidSink &sink_;
  Rect rect_;
  TrackpadConfig cfg_;
  Finger fingers_[kMaxTouches];
  // Gesture state for the pad area.
  int maxFingers_ = 0;
  uint32_t gestureStart_ = 0, lastTapEnd_ = 0;
  bool moved_ = false, dragging_ = false, pendingClick_ = false;
  float fracX_ = 0, fracY_ = 0, scrollAccX_ = 0, scrollAccY_ = 0;
  uint8_t stripHeld_ = 0;
};

}  // namespace t5m
