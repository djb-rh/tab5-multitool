// A macropad: one page of a MacroLayout as a grid of buttons, with a strip of
// page tabs along the bottom when there is more than one page.
//
// A button fires when the finger lifts while still on it, so sliding off
// cancels - a macro is a bigger thing to send by accident than a key.
#pragma once

#include "Macro.h"
#include "Touch.h"

namespace t5m {

class MacroPad {
 public:
  void setLayout(const MacroLayout *layout);
  void setRect(const Rect &r) { rect_ = r; dirty_ = true; }
  const Rect &rect() const { return rect_; }
  void setPage(int i);
  int page() const { return page_; }
  int pageCount() const { return layout_ ? (int)layout_->pages.size() : 0; }

  void handle(const TouchPoint *touches, int n);
  // The next button tapped, if any.
  bool next(const MacroButton **out);

  // Drawing support.
  bool buttonRect(int i, Rect *r) const;  // i-th slot on the current page
  bool tabRect(int i, Rect *r) const;     // page tabs; false with one page
  int pressed() const { return pressed_; }
  int pressedTab() const { return pressedTab_; }
  const MacroPage *current() const;
  const char *pageName(int i) const { return i < pageCount() ? layout_->pages[i].name.c_str() : ""; }
  bool dirty() const { return dirty_; }
  void clean() { dirty_ = false; }
  static constexpr int kTabH = 60;

 private:
  int hitButton(int x, int y) const;
  int hitTab(int x, int y) const;
  Rect grid() const;

  const MacroLayout *layout_ = nullptr;
  Rect rect_;
  int page_ = 0;
  int finger_ = -1;        // the touch id driving the pad (one at a time)
  int pressed_ = -1;       // button under it, -1 none
  int pressedTab_ = -1;
  int downButton_ = -1, downTab_ = -1;
  const MacroButton *fired_[8] = {};
  int head_ = 0, tail_ = 0;
  bool dirty_ = true;
};

}  // namespace t5m
