// On-screen keyboards: QWERTY, symbols + F-keys, numpad, nav + media.
//
// A layout is rows of keys with widths in quarter units; the keypad scales
// it into whatever rectangle it is given, so "full screen" is just a bigger
// rectangle. Keys go down when a finger lands on them and up when it lifts
// (sliding off doesn't drop or retarget a key), so held keys repeat on the
// host. Shift/Ctrl/Alt/GUI tap to latch for the next key, double-tap to lock.
//
// Output is the same KeyEvent the Tab5 Keyboard produces: send it to a
// HidSink, or turn it into text with KeyText for local entry.
#pragma once

#include <cstddef>
#include <cstdint>

#include "Touch.h"
#include "hid_usage.h"

namespace t5m {

struct KeyDef {
  enum Kind : uint8_t { Key, Mod, Page, Media, Gap };
  const char *label;
  uint16_t code;      // Key: usage; Mod: mod bit; Page: page index; Media: consumer usage
  uint8_t width = 4;  // quarter units (4 = one normal key)
  Kind kind = Key;
  uint8_t mods = 0;   // Key: modifiers it always carries (e.g. Shift for '!')
};

struct KeyRow {
  const KeyDef *keys;
  uint8_t count;
};

struct KeyPage {
  const char *name;
  const KeyRow *rows;
  uint8_t count;
};

// Built-in pages.
namespace pages {
extern const KeyPage Qwerty, Function, Numpad, NavMedia;
// Page key codes: which built-in page a Page key shows.
enum Id : uint16_t { kQwerty, kFunction, kNumpad, kNavMedia };
}

class Keypad {
 public:
  Keypad() = default;
  // `pages` is the set the Page keys switch between; index 0 shows first.
  void setPages(const KeyPage *const *pages, int count);
  void setRect(const Rect &r) { rect_ = r; layoutDirty_ = true; dirty_ = true; }
  const Rect &rect() const { return rect_; }
  void setPage(int index);
  int page() const { return page_; }

  // Feed every touch frame (all fingers on the screen; ones outside the
  // keypad are ignored unless they started on a key).
  void handle(const TouchPoint *touches, int n);
  bool next(KeyEvent *out);
  // Media keys come out separately: usage while held, then 0.
  bool nextMedia(uint16_t *usage);

  // Host LEDs (Num/Caps lock) shown on the keys that own them.
  void setHostLeds(uint8_t leds) { if (leds != leds_) { leds_ = leds; dirty_ = true; } }

  // Drawing support for any renderer.
  int keyCount() const;
  bool keyAt(int i, KeyDef *def, Rect *r, bool *lit) const;
  // What key i shows right now: letters in capitals under Shift or Caps Lock,
  // the shifted character on number/symbol keys under Shift.
  const char *labelFor(int i, char *buf, size_t n) const;
  bool dirty() const { return dirty_; }
  void clean() { dirty_ = false; }

 private:
  struct Finger {
    int id = -1;
    int key = -1;
  };
  void layout() const;
  int hit(int x, int y) const;
  void press(int key);
  void release(int key);
  uint8_t currentMods() const { return latched_ | locked_ | heldMods_; }
  void push(const KeyEvent &e);

  const KeyPage *const *pages_ = nullptr;
  int pageCount_ = 0, page_ = 0;
  Rect rect_;
  mutable bool layoutDirty_ = true;
  mutable Rect rects_[96];
  mutable int nkeys_ = 0;
  Finger fingers_[kMaxTouches];
  uint8_t latched_ = 0, locked_ = 0, heldMods_ = 0, modUsed_ = 0;
  uint32_t lastModTap_[8] = {};
  uint8_t leds_ = 0;
  bool dirty_ = true;
  uint8_t sentMods_[96] = {};
  bool pressed_[96] = {};
  static constexpr int kQueue = 32;
  KeyEvent queue_[kQueue];
  int head_ = 0, tail_ = 0;
  uint16_t media_[8] = {};
  int mhead_ = 0, mtail_ = 0;
};

}  // namespace t5m
