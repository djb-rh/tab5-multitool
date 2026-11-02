// USB HID usage codes (keyboard page 0x07, consumer page 0x0C) and modifier
// bits, as the reports on the wire carry them.
#pragma once

#include <cstdint>

namespace t5m {

namespace mod {
constexpr uint8_t LCtrl = 0x01, LShift = 0x02, LAlt = 0x04, LGui = 0x08;
constexpr uint8_t RCtrl = 0x10, RShift = 0x20, RAlt = 0x40, RGui = 0x80;
}  // namespace mod

namespace key {
constexpr uint8_t None = 0x00;
constexpr uint8_t A = 0x04;  // ... Z = 0x1D
constexpr uint8_t N1 = 0x1E;  // ... N9 = 0x26, N0 = 0x27
constexpr uint8_t Enter = 0x28, Escape = 0x29, Backspace = 0x2A, Tab = 0x2B, Space = 0x2C;
constexpr uint8_t Minus = 0x2D, Equal = 0x2E, LeftBrace = 0x2F, RightBrace = 0x30, Backslash = 0x31;
constexpr uint8_t Semicolon = 0x33, Apostrophe = 0x34, Grave = 0x35, Comma = 0x36, Dot = 0x37, Slash = 0x38;
constexpr uint8_t CapsLock = 0x39;
constexpr uint8_t F1 = 0x3A;  // ... F12 = 0x45
constexpr uint8_t PrintScreen = 0x46, ScrollLock = 0x47, Pause = 0x48;
constexpr uint8_t Insert = 0x49, Home = 0x4A, PageUp = 0x4B, Delete = 0x4C, End = 0x4D, PageDown = 0x4E;
constexpr uint8_t Right = 0x4F, Left = 0x50, Down = 0x51, Up = 0x52;
constexpr uint8_t NumLock = 0x53, KpSlash = 0x54, KpAsterisk = 0x55, KpMinus = 0x56, KpPlus = 0x57;
constexpr uint8_t KpEnter = 0x58, Kp1 = 0x59;  // ... Kp9 = 0x61, Kp0 = 0x62
constexpr uint8_t KpDot = 0x63, Application = 0x65, KpEqual = 0x67;
constexpr uint8_t F13 = 0x68;  // ... F24 = 0x73
// 0xE0..0xE7 are the modifiers as keys; they travel as mod bits instead.
constexpr uint8_t LCtrl = 0xE0, LShift = 0xE1, LAlt = 0xE2, LGui = 0xE3;
constexpr uint8_t RCtrl = 0xE4, RShift = 0xE5, RAlt = 0xE6, RGui = 0xE7;
inline constexpr uint8_t f(int n) { return n <= 12 ? (uint8_t)(F1 + n - 1) : (uint8_t)(F13 + n - 13); }
}  // namespace key

namespace consumer {
constexpr uint16_t PlayPause = 0x00CD, Stop = 0x00B7, Next = 0x00B5, Prev = 0x00B6;
constexpr uint16_t Mute = 0x00E2, VolumeUp = 0x00E9, VolumeDown = 0x00EA;
constexpr uint16_t BrightnessUp = 0x006F, BrightnessDown = 0x0070;
}  // namespace consumer

namespace led {
constexpr uint8_t NumLock = 0x01, CapsLock = 0x02, ScrollLock = 0x04;
}

namespace button {
constexpr uint8_t Left = 0x01, Right = 0x02, Middle = 0x04, Back = 0x08, Forward = 0x10;
}

// One key going down or up, whatever produced it: the clip-on keyboard, an
// on-screen key, a macro. `mods` is the modifier state that goes with it.
struct KeyEvent {
  uint8_t usage = 0;
  uint8_t mods = 0;
  bool down = false;
};

// The US-layout usage (and whether Shift is needed) for a printable ASCII
// character; false for anything without a key.
bool asciiToUsage(char c, uint8_t *usage, bool *shift);

}  // namespace t5m
