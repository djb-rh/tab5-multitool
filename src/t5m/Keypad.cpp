#include "Keypad.h"

#include <Arduino.h>

namespace t5m {
namespace pages {
namespace {

using K = KeyDef;
constexpr auto M = KeyDef::Mod;
constexpr auto P = KeyDef::Page;
constexpr auto D = KeyDef::Media;
constexpr auto G = KeyDef::Gap;
constexpr uint8_t SH = mod::LShift;
using namespace key;

// ---- QWERTY ----
const K q0[] = {{"`", Grave}, {"1", N1}, {"2", 0x1F}, {"3", 0x20}, {"4", 0x21}, {"5", 0x22}, {"6", 0x23},
                {"7", 0x24}, {"8", 0x25}, {"9", 0x26}, {"0", 0x27}, {"-", Minus}, {"=", Equal},
                {"Bksp", Backspace, 7}};
const K q1[] = {{"Tab", Tab, 6}, {"q", 0x14}, {"w", 0x1A}, {"e", 0x08}, {"r", 0x15}, {"t", 0x17}, {"y", 0x1C},
                {"u", 0x18}, {"i", 0x0C}, {"o", 0x12}, {"p", 0x13}, {"[", LeftBrace}, {"]", RightBrace},
                {"\\", Backslash, 5}};
const K q2[] = {{"Caps", CapsLock, 7}, {"a", 0x04}, {"s", 0x16}, {"d", 0x07}, {"f", 0x09}, {"g", 0x0A},
                {"h", 0x0B}, {"j", 0x0D}, {"k", 0x0E}, {"l", 0x0F}, {";", Semicolon}, {"'", Apostrophe},
                {"Enter", Enter, 8}};
const K q3[] = {{"Shift", mod::LShift, 9, M}, {"z", 0x1D}, {"x", 0x1B}, {"c", 0x06}, {"v", 0x19}, {"b", 0x05},
                {"n", 0x11}, {"m", 0x10}, {",", Comma}, {".", Dot}, {"/", Slash}, {"Up", Up},
                {"Shift", mod::RShift, 6, M}};
const K q4[] = {{"Fn", kFunction, 6, P}, {"Ctrl", mod::LCtrl, 5, M}, {"Alt", mod::LAlt, 5, M},
                {"Cmd", mod::LGui, 5, M}, {"", Space, 22}, {"Esc", Escape}, {"Left", Left},
                {"Down", Down}, {"Right", Right}};
const KeyRow qRows[] = {{q0, 14}, {q1, 14}, {q2, 13}, {q3, 13}, {q4, 9}};

// ---- Symbols + F-keys + paging ----
const K f0[] = {{"Esc", Escape}, {"F1", 0x3A}, {"F2", 0x3B}, {"F3", 0x3C}, {"F4", 0x3D}, {"F5", 0x3E},
                {"F6", 0x3F}, {"F7", 0x40}, {"F8", 0x41}, {"F9", 0x42}, {"F10", 0x43}, {"F11", 0x44},
                {"F12", 0x45}, {"PrtSc", PrintScreen}};
const K f1[] = {{"~", Grave, 4, K::Key, SH}, {"!", N1, 4, K::Key, SH}, {"@", 0x1F, 4, K::Key, SH},
                {"#", 0x20, 4, K::Key, SH}, {"$", 0x21, 4, K::Key, SH}, {"%", 0x22, 4, K::Key, SH},
                {"^", 0x23, 4, K::Key, SH}, {"&", 0x24, 4, K::Key, SH}, {"*", 0x25, 4, K::Key, SH},
                {"(", 0x26, 4, K::Key, SH}, {")", 0x27, 4, K::Key, SH}, {"_", Minus, 4, K::Key, SH},
                {"+", Equal, 4, K::Key, SH}, {"Bksp", Backspace, 7}};
const K f2[] = {{"Tab", Tab, 6}, {"Ins", Insert}, {"Home", Home}, {"PgUp", PageUp}, {"", 0, 2, G},
                {"{", LeftBrace, 4, K::Key, SH}, {"}", RightBrace, 4, K::Key, SH}, {"|", Backslash, 4, K::Key, SH},
                {":", Semicolon, 4, K::Key, SH}, {"\"", Apostrophe, 4, K::Key, SH}, {"", 0, 2, G},
                {"Enter", Enter, 9}};
const K f3[] = {{"Shift", mod::LShift, 9, M}, {"Del", Delete}, {"End", End}, {"PgDn", PageDown}, {"", 0, 2, G},
                {"<", Comma, 4, K::Key, SH}, {">", Dot, 4, K::Key, SH}, {"?", Slash, 4, K::Key, SH},
                {"Pause", Pause}, {"Menu", Application}, {"Up", Up}, {"", 0, 6, G}};
const K f4[] = {{"abc", kQwerty, 6, P}, {"Ctrl", mod::LCtrl, 5, M}, {"Alt", mod::LAlt, 5, M},
                {"Cmd", mod::LGui, 5, M}, {"", Space, 18}, {"123", kNumpad, 4, P}, {"Left", Left},
                {"Down", Down}, {"Right", Right}};
const KeyRow fRows[] = {{f0, 14}, {f1, 14}, {f2, 12}, {f3, 12}, {f4, 9}};

// ---- Numpad ----
const K n0[] = {{"Num/Clr", NumLock}, {"/", KpSlash}, {"*", KpAsterisk}, {"-", KpMinus}};
const K n1[] = {{"7", Kp1 + 6}, {"8", Kp1 + 7}, {"9", Kp1 + 8}, {"+", KpPlus}};
const K n2[] = {{"4", Kp1 + 3}, {"5", Kp1 + 4}, {"6", Kp1 + 5}, {"Bksp", Backspace}};
const K n3[] = {{"1", Kp1}, {"2", Kp1 + 1}, {"3", Kp1 + 2}, {"Enter", KpEnter}};
const K n4[] = {{"0", Kp1 + 9, 8}, {".", KpDot}, {"Tab", Tab}};
const KeyRow nRows[] = {{n0, 4}, {n1, 4}, {n2, 4}, {n3, 4}, {n4, 3}};

// ---- Navigation + media ----
const K m0[] = {{"Esc", Escape}, {"Ins", Insert}, {"Home", Home}, {"PgUp", PageUp}, {"", 0, 2, G},
                {"Mute", consumer::Mute, 4, D}, {"Vol-", consumer::VolumeDown, 4, D},
                {"Vol+", consumer::VolumeUp, 4, D}};
const K m1[] = {{"Tab", Tab}, {"Del", Delete}, {"End", End}, {"PgDn", PageDown}, {"", 0, 2, G},
                {"Prev", consumer::Prev, 4, D}, {"Play", consumer::PlayPause, 4, D},
                {"Next", consumer::Next, 4, D}};
const K m2[] = {{"", 0, 4, G}, {"", 0, 4, G}, {"Up", Up}, {"", 0, 4, G}, {"", 0, 2, G},
                {"Bri-", consumer::BrightnessDown, 4, D}, {"Bri+", consumer::BrightnessUp, 4, D},
                {"Enter", Enter}};
const K m3[] = {{"", 0, 4, G}, {"Left", Left}, {"Down", Down}, {"Right", Right}, {"", 0, 2, G},
                {"abc", kQwerty, 4, P}, {"Fn", kFunction, 4, P}, {"123", kNumpad, 4, P}};
const KeyRow mRows[] = {{m0, 8}, {m1, 8}, {m2, 8}, {m3, 8}};

}  // namespace

const KeyPage Qwerty = {"abc", qRows, 5};
const KeyPage Function = {"Fn", fRows, 5};
const KeyPage Numpad = {"123", nRows, 5};
const KeyPage NavMedia = {"Nav", mRows, 4};

}  // namespace pages

namespace {
const KeyPage *const kPageById[] = {&pages::Qwerty, &pages::Function, &pages::Numpad, &pages::NavMedia};
constexpr int kGapPx = 6;
constexpr uint32_t kDoubleTapMs = 400;
}  // namespace

void Keypad::setPages(const KeyPage *const *pages, int count) {
  pages_ = pages;
  pageCount_ = count;
  setPage(0);
}

void Keypad::setPage(int index) {
  if (index < 0 || index >= pageCount_) return;
  // A page change mid-press would orphan the held keys: release them first.
  for (auto &f : fingers_) {
    if (f.key >= 0) release(f.key);
    f = Finger();
  }
  page_ = index;
  layoutDirty_ = true;
  dirty_ = true;
}

void Keypad::layout() const {
  if (!layoutDirty_) return;
  layoutDirty_ = false;
  nkeys_ = 0;
  if (!pages_ || pageCount_ == 0) return;
  const KeyPage &p = *pages_[page_];
  const int rowH = rect_.h / p.count;
  for (int r = 0; r < p.count; r++) {
    const KeyRow &row = p.rows[r];
    int units = 0;
    for (int k = 0; k < row.count; k++) units += row.keys[k].width;
    int acc = 0;
    for (int k = 0; k < row.count && nkeys_ < 96; k++) {
      const int x0 = rect_.x + rect_.w * acc / units;
      acc += row.keys[k].width;
      const int x1 = rect_.x + rect_.w * acc / units;
      Rect &kr = rects_[nkeys_++];
      kr.x = x0 + kGapPx / 2;
      kr.y = rect_.y + r * rowH + kGapPx / 2;
      kr.w = x1 - x0 - kGapPx;
      kr.h = rowH - kGapPx;
    }
  }
}

int Keypad::keyCount() const {
  layout();
  return nkeys_;
}

static const KeyDef *defAt(const KeyPage &p, int index) {
  for (int r = 0; r < p.count; r++) {
    if (index < p.rows[r].count) return &p.rows[r].keys[index];
    index -= p.rows[r].count;
  }
  return nullptr;
}

bool Keypad::keyAt(int i, KeyDef *def, Rect *r, bool *lit) const {
  layout();
  if (i < 0 || i >= nkeys_) return false;
  const KeyDef *d = defAt(*pages_[page_], i);
  if (!d) return false;
  *def = *d;
  *r = rects_[i];
  bool on = pressed_[i];
  if (d->kind == KeyDef::Mod) on = on || ((latched_ | locked_) & d->code);
  if (d->kind == KeyDef::Key && d->code == key::CapsLock) on = on || (leds_ & led::CapsLock);
  if (d->kind == KeyDef::Key && d->code == key::NumLock) on = on || (leds_ & led::NumLock);
  *lit = on;
  return true;
}

const char *Keypad::labelFor(int i, char *buf, size_t n) const {
  layout();
  const KeyDef *d = (i >= 0 && i < nkeys_) ? defAt(*pages_[page_], i) : nullptr;
  if (!d || !d->label) return "";
  // Only single-character keys that carry no Shift of their own change.
  if (d->kind != KeyDef::Key || !d->label[0] || d->label[1] || d->mods || n < 2) return d->label;
  const bool shift = currentMods() & (mod::LShift | mod::RShift);
  const bool caps = leds_ & led::CapsLock;
  if (d->code >= key::A && d->code < key::A + 26) {
    buf[0] = (char)((shift != caps ? 'A' : 'a') + d->code - key::A);
    buf[1] = 0;
    return buf;
  }
  if (!shift) return d->label;
  for (int c = 33; c < 127; c++) {
    uint8_t u;
    bool s;
    if (asciiToUsage((char)c, &u, &s) && s && u == d->code) {
      buf[0] = (char)c;
      buf[1] = 0;
      return buf;
    }
  }
  return d->label;
}

int Keypad::hit(int x, int y) const {
  layout();
  if (!rect_.contains(x, y)) return -1;
  for (int i = 0; i < nkeys_; i++) {
    // Gaps between keys belong to the nearest key: grow each by half a gap.
    Rect r = rects_[i];
    r.x -= kGapPx / 2; r.y -= kGapPx / 2; r.w += kGapPx; r.h += kGapPx;
    if (r.contains(x, y)) {
      const KeyDef *d = defAt(*pages_[page_], i);
      return d && d->kind != KeyDef::Gap ? i : -1;
    }
  }
  return -1;
}

void Keypad::handle(const TouchPoint *touches, int n) {
  // Fingers that lifted: release their keys.
  for (auto &f : fingers_) {
    if (f.id < 0) continue;
    bool still = false;
    for (int i = 0; i < n; i++) if (touches[i].down && touches[i].id == f.id) still = true;
    if (!still) {
      if (f.key >= 0) release(f.key);
      f = Finger();
    }
  }
  // New fingers: press what they landed on.
  for (int i = 0; i < n; i++) {
    if (!touches[i].down) continue;
    bool known = false;
    for (auto &f : fingers_) if (f.id == touches[i].id) known = true;
    if (known) continue;
    const int key = hit(touches[i].x, touches[i].y);
    for (auto &f : fingers_) {
      if (f.id >= 0) continue;
      f.id = touches[i].id;
      f.key = key;  // -1 (a miss) still owns the finger until it lifts
      if (key >= 0) press(key);
      break;
    }
  }
}

void Keypad::press(int k) {
  const KeyDef *d = defAt(*pages_[page_], k);
  if (!d) return;
  pressed_[k] = true;
  dirty_ = true;
  switch (d->kind) {
    case KeyDef::Key: {
      const uint8_t m = currentMods() | d->mods;
      sentMods_[k] = m;
      push({(uint8_t)d->code, m, true});
      break;
    }
    case KeyDef::Mod:
      heldMods_ |= d->code;
      modUsed_ &= ~d->code;
      push({0, currentMods(), true});
      break;
    case KeyDef::Media:
      if (((mhead_ + 1) & 7) != mtail_) { media_[mhead_] = d->code; mhead_ = (mhead_ + 1) & 7; }
      break;
    case KeyDef::Page:
    case KeyDef::Gap:
      break;
  }
  // Any key used while a modifier is held makes that modifier a chord, not a tap.
  if (d->kind != KeyDef::Mod) modUsed_ |= heldMods_;
}

void Keypad::release(int k) {
  const KeyDef *d = defAt(*pages_[page_], k);
  if (!d) return;
  pressed_[k] = false;
  dirty_ = true;
  switch (d->kind) {
    case KeyDef::Key:
      push({(uint8_t)d->code, (uint8_t)(latched_ | locked_ | heldMods_), false});
      if (latched_) {  // one-shot modifiers are spent by one key
        latched_ = 0;
        push({0, currentMods(), false});
      }
      break;
    case KeyDef::Mod: {
      const uint8_t bit = d->code;
      heldMods_ &= ~bit;
      if (!(modUsed_ & bit)) {
        // A tap: latch; a second tap soon after: lock; a tap on a lock: off.
        int idx = 0;
        while (idx < 8 && !((bit >> idx) & 1)) idx++;
        const uint32_t now = millis();
        if (locked_ & bit) {
          locked_ &= ~bit;
        } else if ((latched_ & bit) && now - lastModTap_[idx] < kDoubleTapMs) {
          latched_ &= ~bit;
          locked_ |= bit;
        } else {
          latched_ ^= bit;
        }
        lastModTap_[idx] = now;
      }
      push({0, currentMods(), false});
      break;
    }
    case KeyDef::Media:
      if (((mhead_ + 1) & 7) != mtail_) { media_[mhead_] = 0; mhead_ = (mhead_ + 1) & 7; }
      break;
    case KeyDef::Page:
      // Switch on release, so the finger that pressed it isn't carried over.
      for (int i = 0; i < pageCount_; i++) {
        if (d->code < sizeof(kPageById) / sizeof(kPageById[0]) && pages_[i] == kPageById[d->code]) {
          for (auto &f : fingers_) if (f.key == k) f.key = -1;
          setPage(i);
          return;
        }
      }
      break;
    case KeyDef::Gap:
      break;
  }
}

void Keypad::push(const KeyEvent &e) {
  const int next = (head_ + 1) % kQueue;
  if (next == tail_) return;
  queue_[head_] = e;
  head_ = next;
}

bool Keypad::next(KeyEvent *out) {
  if (tail_ == head_) return false;
  *out = queue_[tail_];
  tail_ = (tail_ + 1) % kQueue;
  return true;
}

bool Keypad::nextMedia(uint16_t *usage) {
  if (mtail_ == mhead_) return false;
  *usage = media_[mtail_];
  mtail_ = (mtail_ + 1) & 7;
  return true;
}

}  // namespace t5m
