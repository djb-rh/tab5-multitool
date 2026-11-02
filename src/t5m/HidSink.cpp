#include "HidSink.h"


namespace t5m {

bool asciiToUsage(char c, uint8_t *usage, bool *shift) {
  *shift = false;
  if (c >= 'a' && c <= 'z') { *usage = key::A + (c - 'a'); return true; }
  if (c >= 'A' && c <= 'Z') { *usage = key::A + (c - 'A'); *shift = true; return true; }
  if (c >= '1' && c <= '9') { *usage = key::N1 + (c - '1'); return true; }
  if (c == '0') { *usage = 0x27; return true; }
  static const struct { char c; uint8_t usage; bool shift; } kTable[] = {
      {' ', key::Space, false},       {'\n', key::Enter, false},       {'\t', key::Tab, false},
      {'-', key::Minus, false},       {'_', key::Minus, true},         {'=', key::Equal, false},
      {'+', key::Equal, true},        {'[', key::LeftBrace, false},    {'{', key::LeftBrace, true},
      {']', key::RightBrace, false},  {'}', key::RightBrace, true},    {'\\', key::Backslash, false},
      {'|', key::Backslash, true},    {';', key::Semicolon, false},    {':', key::Semicolon, true},
      {'\'', key::Apostrophe, false}, {'"', key::Apostrophe, true},    {'`', key::Grave, false},
      {'~', key::Grave, true},        {',', key::Comma, false},        {'<', key::Comma, true},
      {'.', key::Dot, false},         {'>', key::Dot, true},           {'/', key::Slash, false},
      {'?', key::Slash, true},        {'!', 0x1E, true},               {'@', 0x1F, true},
      {'#', 0x20, true},              {'$', 0x21, true},               {'%', 0x22, true},
      {'^', 0x23, true},              {'&', 0x24, true},               {'*', 0x25, true},
      {'(', 0x26, true},              {')', 0x27, true},
  };
  for (const auto &e : kTable) {
    if (e.c == c) { *usage = e.usage; *shift = e.shift; return true; }
  }
  return false;
}

void HidSink::apply(const KeyEvent &e) {
  if (e.usage >= 0xE8 && e.usage != 0) return;  // app-only keys (Tab5Keyboard::kAppKey)
  setModifiers(e.mods);
  if (e.usage) {
    if (e.down) press(e.usage);
    else release(e.usage);
  }
}

void HidSink::tap(uint8_t usage, uint8_t mods) {
  if (mods) setModifiers(mods);
  press(usage);
  release(usage);
  if (mods) setModifiers(0);
}

void HidSink::consumerTap(uint16_t usage) {
  consumer(usage);
  consumer(0);
}

void HidSink::click(uint8_t buttons) {
  mouseButtons(buttons);
  mouseButtons(0);
}

int HidSink::typeText(const char *text) {
  int sent = 0;
  for (const char *p = text; *p; ++p) {
    uint8_t usage;
    bool shift;
    if (!asciiToUsage(*p, &usage, &shift)) continue;
    tap(usage, shift ? mod::LShift : 0);
    sent++;
  }
  return sent;
}

}  // namespace t5m
