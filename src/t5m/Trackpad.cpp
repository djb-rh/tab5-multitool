#include "Trackpad.h"

#include <cmath>
#include <cstdlib>

namespace t5m {
namespace {
constexpr uint32_t kTapMs = 220;       // longest touch that still counts as a tap
constexpr int kTapSlopPx = 14;         // and how far it may wander
constexpr uint32_t kDragGapMs = 260;   // tap, then down again within this = drag
}  // namespace

bool Trackpad::padRect(Rect *r) const {
  *r = rect_;
  if (cfg_.buttonStrip > 0) r->h -= cfg_.buttonStrip;
  return true;
}

bool Trackpad::buttonRect(int i, Rect *r) const {
  if (cfg_.buttonStrip <= 0 || i < 0 || i > 2) return false;
  // Left and right are wide, middle is narrow.
  const int w = rect_.w;
  const int edges[4] = {0, w * 2 / 5, w * 3 / 5, w};
  r->x = rect_.x + edges[i];
  r->w = edges[i + 1] - edges[i];
  r->y = rect_.y + rect_.h - cfg_.buttonStrip;
  r->h = cfg_.buttonStrip;
  return true;
}

void Trackpad::handle(const TouchPoint *touches, int n, uint32_t now) {
  // A one-finger tap waits briefly before it clicks, in case it is the start
  // of a tap-and-drag.
  if (pendingClick_ && now - lastTapEnd_ >= kDragGapMs) {
    pendingClick_ = false;
    sink_.click(button::Left);
  }

  // Lifted fingers.
  int padFingersBefore = 0;
  for (auto &f : fingers_) if (f.id >= 0 && !f.stripButton) padFingersBefore++;
  for (auto &f : fingers_) {
    if (f.id < 0) continue;
    bool still = false;
    for (int i = 0; i < n; i++) if (touches[i].down && touches[i].id == f.id) still = true;
    if (still) continue;
    if (f.stripButton) {
      stripHeld_ &= ~f.stripButton;
      sink_.mouseButtons(stripHeld_ | (dragging_ ? button::Left : 0));
    }
    f = Finger();
  }

  // New fingers.
  for (int i = 0; i < n; i++) {
    const TouchPoint &t = touches[i];
    if (!t.down) continue;
    bool known = false;
    for (auto &f : fingers_) if (f.id == t.id) known = true;
    if (known || !rect_.contains(t.x, t.y)) continue;
    for (auto &f : fingers_) {
      if (f.id >= 0) continue;
      f.id = t.id;
      f.x = f.x0 = t.x;
      f.y = f.y0 = t.y;
      f.stripButton = 0;
      for (int b = 0; b < 3; b++) {
        Rect br;
        if (buttonRect(b, &br) && br.contains(t.x, t.y)) {
          f.stripButton = b == 0 ? button::Left : b == 1 ? button::Middle : button::Right;
          stripHeld_ |= f.stripButton;
          sink_.mouseButtons(stripHeld_ | (dragging_ ? button::Left : 0));
        }
      }
      break;
    }
  }

  // Pad fingers now, and how far they moved this frame.
  int count = 0;
  float dx = 0, dy = 0;
  for (auto &f : fingers_) {
    if (f.id < 0 || f.stripButton) continue;
    for (int i = 0; i < n; i++) {
      if (touches[i].id != f.id) continue;
      dx += touches[i].x - f.x;
      dy += touches[i].y - f.y;
      f.x = touches[i].x;
      f.y = touches[i].y;
      if (std::abs(f.x - f.x0) > kTapSlopPx || std::abs(f.y - f.y0) > kTapSlopPx) moved_ = true;
    }
    count++;
  }

  if (padFingersBefore == 0 && count > 0) {
    // A gesture starts. Down again right after a tap: drag.
    gestureStart_ = now;
    maxFingers_ = count;
    moved_ = false;
    fracX_ = fracY_ = scrollAccX_ = scrollAccY_ = 0;
    if (count == 1 && pendingClick_) {
      pendingClick_ = false;
      dragging_ = true;
      sink_.mouseButtons(stripHeld_ | button::Left);
    }
  }
  if (count > maxFingers_) maxFingers_ = count;

  if (count == 1 && maxFingers_ == 1) {
    // Move, with acceleration on fast strokes; keep the remainder so slow
    // strokes still add up.
    const float v = std::sqrt(dx * dx + dy * dy);
    const float gain = cfg_.speed * (1.0f + cfg_.accel * v);
    fracX_ += dx * gain;
    fracY_ += dy * gain;
    const int mx = (int)fracX_, my = (int)fracY_;
    fracX_ -= mx;
    fracY_ -= my;
    if (mx || my) sink_.mouseMove(mx, my);
  } else if (count >= 2) {
    // Scroll by the fingers' average motion.
    const float sign = cfg_.naturalScroll ? 1.0f : -1.0f;
    scrollAccY_ += sign * dy / count;
    scrollAccX_ += -sign * dx / count;
    const int wy = (int)(scrollAccY_ / cfg_.scrollPx), wx = (int)(scrollAccX_ / cfg_.scrollPx);
    scrollAccY_ -= wy * cfg_.scrollPx;
    scrollAccX_ -= wx * cfg_.scrollPx;
    if (wy || wx) sink_.mouseMove(0, 0, wy, wx);
  }

  if (padFingersBefore > 0 && count == 0) endGesture(now);
}

void Trackpad::endGesture(uint32_t now) {
  const bool tap = cfg_.tapToClick && !moved_ && now - gestureStart_ < kTapMs;
  if (dragging_) {
    // The button went down at the start of this touch. A short, still touch
    // makes that the first click of a double-click, and this the second.
    dragging_ = false;
    sink_.mouseButtons(stripHeld_);
    if (tap) sink_.click(button::Left);
    return;
  }
  if (!tap) return;
  if (maxFingers_ == 1) {
    pendingClick_ = true;
    lastTapEnd_ = now;
    return;
  }
  const uint8_t b = maxFingers_ >= 3 ? button::Middle : button::Right;
  sink_.mouseButtons(stripHeld_ | b);
  sink_.mouseButtons(stripHeld_);
}

}  // namespace t5m
