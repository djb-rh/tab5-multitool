#include "Tab5Keyboard.h"

#include <M5Unified.h>
#include <driver/gpio.h>

namespace t5m {
namespace {

constexpr uint8_t kAddress = 0x6D;
constexpr int kSda = 0, kScl = 1, kInt = 50;
constexpr uint32_t kFreq = 400000;

constexpr uint8_t kRegIntCfg = 0x00, kRegEventNum = 0x02, kRegBrightness = 0x03;
constexpr uint8_t kRegMode = 0x10, kRegRgbMode = 0x11, kRegKeyEvent = 0x20, kRegRgb = 0x60;
constexpr uint8_t kRegFirmware = 0xFE;
constexpr uint8_t kModeKey = 0, kRgbUser = 1;

constexpr uint32_t kProbeMs = 700;   // while absent, how often to look
constexpr uint32_t kCheckMs = 3000;  // while present, how often to make sure
constexpr uint32_t kPollMs = 20;     // poll anyway, in case an INT edge is missed
constexpr uint32_t kDoubleTapMs = 400;

constexpr int kSymRow = 3, kSymCol = 0, kAaRow = 3, kAaCol = 1, kCtrlRow = 4, kCtrlCol = 0, kAltRow = 4, kAltCol = 1;

// One key: its usage and Shift as printed, and with Sym held. Taken from M5's
// keyboard firmware (user_keyboard_handle.c, MIT), where Sym's legend exists;
// where it doesn't, Sym gives an Fn layer instead (F-keys, paging, Insert).
struct Legend {
  uint8_t usage, shift, sym_usage, sym_shift;
};
constexpr uint8_t S = 1;
constexpr uint8_t kAppKeyMarker = 0xF0;  // Sym+Esc: for the app, never sent
using namespace key;
const Legend kMap[5][14] = {
    // esc 1 2 3 4 5 6 7 8 9 0 - + del   (Sym: F1..F12 on 1..+, Insert on del)
    {{Escape, 0, kAppKeyMarker, 0}, {N1, 0, f(1), 0}, {0x1F, 0, f(2), 0}, {0x20, 0, f(3), 0},
     {0x21, 0, f(4), 0}, {0x22, 0, f(5), 0}, {0x23, 0, f(6), 0}, {0x24, 0, f(7), 0},
     {0x25, 0, f(8), 0}, {0x26, 0, f(9), 0}, {0x27, 0, f(10), 0}, {Minus, 0, f(11), 0},
     {Equal, S, f(12), 0}, {Delete, 0, Insert, 0}},
    // ` ! @ # $ % ^ & * ( ) [ ] \   (Sym: ~ ? @ # $ % ^ & / < > { } |)
    {{Grave, 0, Grave, S}, {N1, S, Slash, S}, {0x1F, S, 0x1F, S}, {0x20, S, 0x20, S},
     {0x21, S, 0x21, S}, {0x22, S, 0x22, S}, {0x23, S, 0x23, S}, {0x24, S, 0x24, S},
     {0x25, S, Slash, 0}, {0x26, S, Comma, S}, {0x27, S, Dot, S}, {LeftBrace, 0, LeftBrace, S},
     {RightBrace, 0, RightBrace, S}, {Backslash, 0, Backslash, S}},
    // tab q w e r t y u i o p ; ' backspace   (Sym: ; -> :, ' -> ")
    {{Tab, 0, Tab, 0}, {0x14, 0, 0x14, 0}, {0x1A, 0, 0x1A, 0}, {0x08, 0, 0x08, 0},
     {0x15, 0, 0x15, 0}, {0x17, 0, 0x17, 0}, {0x1C, 0, 0x1C, 0}, {0x18, 0, 0x18, 0},
     {0x0C, 0, 0x0C, 0}, {0x12, 0, 0x12, 0}, {0x13, 0, 0x13, 0}, {Semicolon, 0, Semicolon, S},
     {Apostrophe, 0, Apostrophe, S}, {Backspace, 0, Backspace, 0}},
    // sym Aa a s d f g h j k l up _ enter   (Sym: up -> PageUp, _ -> =)
    {{0, 0, 0, 0}, {0, 0, 0, 0}, {0x04, 0, 0x04, 0}, {0x16, 0, 0x16, 0},
     {0x07, 0, 0x07, 0}, {0x09, 0, 0x09, 0}, {0x0A, 0, 0x0A, 0}, {0x0B, 0, 0x0B, 0},
     {0x0D, 0, 0x0D, 0}, {0x0E, 0, 0x0E, 0}, {0x0F, 0, 0x0F, 0}, {Up, 0, PageUp, 0},
     {Minus, S, Equal, 0}, {Enter, 0, Enter, 0}},
    // ctrl alt z x c v b n m . left down right space   (Sym: . -> , and Home/PageDown/End)
    {{0, 0, 0, 0}, {0, 0, 0, 0}, {0x1D, 0, 0x1D, 0}, {0x1B, 0, 0x1B, 0},
     {0x06, 0, 0x06, 0}, {0x19, 0, 0x19, 0}, {0x05, 0, 0x05, 0}, {0x11, 0, 0x11, 0},
     {0x10, 0, 0x10, 0}, {Dot, 0, Comma, 0}, {Left, 0, Home, 0}, {Down, 0, PageDown, 0},
     {Right, 0, End, 0}, {Space, 0, Space, 0}},
};

}  // namespace

static_assert(kAppKeyMarker == Tab5Keyboard::kAppKey, "app key marker");

bool Tab5Keyboard::bus() {
  if (bus_) return true;
  M5.Ex_I2C.release();
  bus_ = M5.Ex_I2C.begin(I2C_NUM_0, kSda, kScl);
  if (bus_) {
    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << kInt;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io);
  }
  return bus_;
}

bool Tab5Keyboard::answers() {
  // A bare address probe; readRegister8 returns a byte whether or not anyone is there.
  const bool ok = M5.Ex_I2C.start(kAddress, false, kFreq) && M5.Ex_I2C.write(kRegFirmware);
  M5.Ex_I2C.stop();
  return ok;
}

void Tab5Keyboard::begin() {
  next_probe_ = 0;
  bus();
}

void Tab5Keyboard::found() {
  present_ = true;
  M5.Ex_I2C.writeRegister8(kAddress, kRegMode, kModeKey, kFreq);
  M5.Ex_I2C.writeRegister8(kAddress, kRegEventNum, 0, kFreq);  // drop what queued unseen
  M5.Ex_I2C.writeRegister8(kAddress, kRegIntCfg, 0x07, kFreq);
  firmware_ = M5.Ex_I2C.readRegister8(kAddress, kRegFirmware, kFreq);
  memset(held_, 0, sizeof held_);
  sym_ = aa_ = ctrl_ = alt_ = false;
  leds_dirty_ = true;
}

void Tab5Keyboard::tick() {
  if (!bus()) return;
  const uint32_t now = millis();
  if (!present_) {
    if ((int32_t)(now - next_probe_) < 0) return;
    next_probe_ = now + kProbeMs;
    if (answers()) found();
    return;
  }
  if ((int32_t)(now - next_check_) >= 0) {
    next_check_ = now + kCheckMs;
    if (!answers()) {
      // Unplugged: whatever was held is released.
      for (int r = 0; r < 5; r++)
        for (int c = 0; c < 14; c++)
          if (held(r, c)) handle(r, c, false);
      present_ = false;
      return;
    }
  }
  if (leds_dirty_) applyLeds();
  // INT is active low while events are queued.
  if (gpio_get_level((gpio_num_t)kInt) != 0 && (int32_t)(now - next_poll_) < 0) return;
  next_poll_ = now + kPollMs;
  readEvents();
}

void Tab5Keyboard::readEvents() {
  const uint8_t n = M5.Ex_I2C.readRegister8(kAddress, kRegEventNum, kFreq);
  if (n == 0 || n > 32) return;
  for (int i = 0; i < n; i++) {
    const uint8_t v = M5.Ex_I2C.readRegister8(kAddress, kRegKeyEvent, kFreq);
    if (v == 0xFF) return;  // the queue ran dry
    const int row = (v >> 4) & 7, col = v & 0x0F;
    if (row < 5 && col < 14) handle(row, col, (v & 0x80) != 0);
  }
}

void Tab5Keyboard::handle(int row, int col, bool down) {
  const bool was = held(row, col);
  if (was == down) return;
  if (down) held_[row] |= 1 << col;
  else held_[row] &= ~(1 << col);

  auto mods = [this]() {
    uint8_t m = 0;
    if (aa_) m |= mod::LShift;
    if (ctrl_) m |= mod::LCtrl;
    if (alt_) m |= sym_ ? mod::LGui : mod::LAlt;
    return m;
  };

  if (row == kSymRow && col == kSymCol) {
    sym_ = down;
    if (alt_) push({0, mods(), down});  // Alt <-> GUI follows Sym
    return;
  }
  if (row == kAaRow && col == kAaCol) {
    const uint32_t now = millis();
    if (down) {
      aa_ = true;
      aa_other_ = false;
      aa_down_at_ = now;
      push({0, mods(), true});
    } else {
      aa_ = false;
      push({0, mods(), false});
      // A tap, then another tap soon after: Caps Lock.
      if (!aa_other_ && now - aa_down_at_ < kDoubleTapMs) {
        if (now - aa_last_tap_ < kDoubleTapMs) {
          push({CapsLock, mods(), true});
          push({CapsLock, mods(), false});
          aa_last_tap_ = 0;
        } else {
          aa_last_tap_ = now;
        }
      }
    }
    return;
  }
  if (row == kCtrlRow && col == kCtrlCol) { ctrl_ = down; push({0, mods(), down}); return; }
  if (row == kAltRow && col == kAltCol) { alt_ = down; push({0, mods(), down}); return; }

  if (down) {
    aa_other_ = true;
    const Legend &l = kMap[row][col];
    const uint8_t usage = sym_ ? l.sym_usage : l.usage;
    const bool shift = sym_ ? l.sym_shift : l.shift;
    if (!usage) return;
    sent_usage_[row][col] = usage;
    push({usage, (uint8_t)(mods() | (shift ? mod::LShift : 0)), true});
  } else {
    const uint8_t usage = sent_usage_[row][col];
    if (!usage) return;
    sent_usage_[row][col] = 0;
    push({usage, mods(), false});
  }
}

void Tab5Keyboard::push(const KeyEvent &e) {
  const int next = (head_ + 1) % kQueue;
  if (next == tail_) return;
  queue_[head_] = e;
  head_ = next;
}

bool Tab5Keyboard::next(KeyEvent *out) {
  if (tail_ == head_) return false;
  *out = queue_[tail_];
  tail_ = (tail_ + 1) % kQueue;
  return true;
}

void Tab5Keyboard::setLed(int index, uint8_t r, uint8_t g, uint8_t b) {
  if (index < 0 || index > 1) return;
  if (leds_[index][0] == r && leds_[index][1] == g && leds_[index][2] == b) return;
  leds_[index][0] = r;
  leds_[index][1] = g;
  leds_[index][2] = b;
  leds_dirty_ = true;
}

void Tab5Keyboard::setLedBrightness(uint8_t percent) {
  if (percent > 100) percent = 100;
  if (percent == led_brightness_) return;
  led_brightness_ = percent;
  leds_dirty_ = true;
}

void Tab5Keyboard::applyLeds() {
  // Registers hold B, G, R, pad for each LED.
  const uint8_t buf[8] = {leds_[0][2], leds_[0][1], leds_[0][0], 0, leds_[1][2], leds_[1][1], leds_[1][0], 0};
  M5.Ex_I2C.writeRegister8(kAddress, kRegBrightness, led_brightness_, kFreq);
  M5.Ex_I2C.writeRegister(kAddress, kRegRgb, buf, sizeof buf, kFreq);
  M5.Ex_I2C.writeRegister8(kAddress, kRegRgbMode, kRgbUser, kFreq);  // our colours, not its status
  leds_dirty_ = false;
}

bool KeyText::from(const KeyEvent &e, KeyText *out) {
  if (!e.down || !e.usage || e.usage == Tab5Keyboard::kAppKey) return false;
  *out = KeyText();
  out->ctrl = e.mods & (mod::LCtrl | mod::RCtrl);
  out->alt = e.mods & (mod::LAlt | mod::RAlt);
  const bool shift = e.mods & (mod::LShift | mod::RShift);
  // Printable characters: search the ASCII table backwards.
  for (int c = 32; c < 127; c++) {
    uint8_t u;
    bool s;
    if (asciiToUsage((char)c, &u, &s) && u == e.usage && s == shift) {
      out->ch = (char)c;
      return true;
    }
  }
  if (e.usage >= key::A && e.usage <= key::A + 25) {  // letters ignore Shift mismatch above
    out->ch = (char)((shift ? 'A' : 'a') + e.usage - key::A);
    return true;
  }
  if (e.usage == key::Enter || e.usage == key::Escape || e.usage == key::Backspace || e.usage == key::Tab ||
      (e.usage >= key::F1 && e.usage <= key::Up)) {
    out->special = e.usage;
    return true;
  }
  return false;
}

}  // namespace t5m
