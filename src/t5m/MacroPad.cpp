#include "MacroPad.h"

namespace t5m {
namespace {
constexpr int kGap = 10;
}

void MacroPad::setLayout(const MacroLayout *layout) {
  layout_ = layout;
  if (page_ >= pageCount()) page_ = 0;
  finger_ = pressed_ = pressedTab_ = downButton_ = downTab_ = -1;
  head_ = tail_ = 0;  // queued taps point into the old layout
  dirty_ = true;
}

void MacroPad::setPage(int i) {
  if (i < 0 || i >= pageCount() || i == page_) return;
  page_ = i;
  pressed_ = downButton_ = -1;
  dirty_ = true;
}

const MacroPage *MacroPad::current() const {
  return pageCount() ? &layout_->pages[page_] : nullptr;
}

Rect MacroPad::grid() const {
  Rect g = rect_;
  if (pageCount() > 1) g.h -= kTabH;
  return g;
}

bool MacroPad::buttonRect(int i, Rect *r) const {
  const MacroPage *p = current();
  if (!p || i < 0 || i >= p->cols * p->rows) return false;
  const Rect g = grid();
  const int c = i % p->cols, row = i / p->cols;
  const int x0 = g.x + g.w * c / p->cols, x1 = g.x + g.w * (c + 1) / p->cols;
  const int y0 = g.y + g.h * row / p->rows, y1 = g.y + g.h * (row + 1) / p->rows;
  *r = {x0 + kGap / 2, y0 + kGap / 2, x1 - x0 - kGap, y1 - y0 - kGap};
  return true;
}

bool MacroPad::tabRect(int i, Rect *r) const {
  const int n = pageCount();
  if (n < 2 || i < 0 || i >= n) return false;
  const int y = rect_.y + rect_.h - kTabH;
  const int x0 = rect_.x + rect_.w * i / n, x1 = rect_.x + rect_.w * (i + 1) / n;
  *r = {x0 + kGap / 2, y + kGap / 2, x1 - x0 - kGap, kTabH - kGap};
  return true;
}

int MacroPad::hitButton(int x, int y) const {
  const MacroPage *p = current();
  if (!p) return -1;
  for (int i = 0; i < p->cols * p->rows; i++) {
    Rect r;
    if (buttonRect(i, &r) && r.contains(x, y)) {
      // Empty slots (no steps) don't press.
      return i < (int)p->buttons.size() && !p->buttons[i].steps.empty() ? i : -1;
    }
  }
  return -1;
}

int MacroPad::hitTab(int x, int y) const {
  for (int i = 0; i < pageCount(); i++) {
    Rect r;
    if (tabRect(i, &r) && r.contains(x, y)) return i;
  }
  return -1;
}

void MacroPad::handle(const TouchPoint *touches, int n) {
  const TouchPoint *f = nullptr;
  for (int i = 0; i < n; i++) if (touches[i].down && touches[i].id == finger_) f = &touches[i];

  if (finger_ >= 0 && !f) {
    // Lifted: fire if it ended where it started.
    if (downButton_ >= 0 && pressed_ == downButton_) {
      const int next = (head_ + 1) % 8;
      if (next != tail_) {
        fired_[head_] = &layout_->pages[page_].buttons[downButton_];
        head_ = next;
      }
    }
    if (downTab_ >= 0 && pressedTab_ == downTab_) setPage(downTab_);
    finger_ = downButton_ = downTab_ = -1;
    if (pressed_ >= 0 || pressedTab_ >= 0) dirty_ = true;
    pressed_ = pressedTab_ = -1;
    return;
  }
  if (finger_ < 0) {
    // A new finger inside the pad takes it.
    for (int i = 0; i < n; i++) {
      if (!touches[i].down || !rect_.contains(touches[i].x, touches[i].y)) continue;
      finger_ = touches[i].id;
      downButton_ = pressed_ = hitButton(touches[i].x, touches[i].y);
      downTab_ = pressedTab_ = hitTab(touches[i].x, touches[i].y);
      dirty_ = true;
      return;
    }
    return;
  }
  // Still down: track whether it is over the button it started on.
  const int b = downButton_ >= 0 && hitButton(f->x, f->y) == downButton_ ? downButton_ : -1;
  const int t = downTab_ >= 0 && hitTab(f->x, f->y) == downTab_ ? downTab_ : -1;
  if (b != pressed_ || t != pressedTab_) {
    pressed_ = b;
    pressedTab_ = t;
    dirty_ = true;
  }
}

bool MacroPad::next(const MacroButton **out) {
  if (tail_ == head_) return false;
  *out = fired_[tail_];
  tail_ = (tail_ + 1) % 8;
  return true;
}

}  // namespace t5m
