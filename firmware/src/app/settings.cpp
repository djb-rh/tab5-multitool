#include "settings.h"

#include <Arduino.h>
#include <Preferences.h>

namespace mt {

Settings g_settings;

namespace {
bool g_dirty = false;
bool g_found = false;
uint32_t g_changed_at = 0;
constexpr uint32_t kSaveDelayMs = 3000;
}  // namespace

void loadSettings() {
  Preferences p;
  if (!p.begin("multitool", true)) return;
  g_found = true;
  Settings &s = g_settings;
  s.app = (App)p.getUChar("app", (uint8_t)s.app);
  if (s.app >= App::kCount) s.app = App::Keyboard;
  s.link = (Link)p.getUChar("link", (uint8_t)s.link);
  s.touchMode = (Mode)p.getUChar("tmode", (uint8_t)s.touchMode);
  s.keyboardMode = (Mode)p.getUChar("kmode", (uint8_t)s.keyboardMode);
  s.pointerSpeed = p.getUChar("speed", s.pointerSpeed);
  s.naturalScroll = p.getBool("natural", s.naturalScroll);
  s.tapToClick = p.getBool("tap", s.tapToClick);
  // Version 2 dimmed the LED default (6 -> 3 %); older saves get the new one.
  if (p.getUChar("ver", 1) >= 2) s.ledBrightness = p.getUChar("led", s.ledBrightness);
  s.wifi = p.getBool("wifi", s.wifi);
  s.webEditor = p.getBool("web", s.webEditor);
  s.screenBrightness = p.getUChar("bright", s.screenBrightness);
  s.bleIdentity = p.getUChar("bleid", s.bleIdentity);
  p.end();
  if ((uint8_t)s.touchMode >= (uint8_t)Mode::kCount) s.touchMode = Mode::KeyboardMouse;
  if ((uint8_t)s.keyboardMode >= (uint8_t)Mode::kCount) s.keyboardMode = Mode::PadNumpad;
  if ((uint8_t)s.link > (uint8_t)Link::Both) s.link = Link::Usb;
}

bool settingsFound() { return g_found; }

void settingsChanged() {
  g_dirty = true;
  g_changed_at = millis();
}

void settingsTick(uint32_t now) {
  if (!g_dirty || now - g_changed_at < kSaveDelayMs) return;
  g_dirty = false;
  Preferences p;
  if (!p.begin("multitool", false)) return;
  const Settings &s = g_settings;
  p.putUChar("app", (uint8_t)s.app);
  p.putUChar("link", (uint8_t)s.link);
  p.putUChar("tmode", (uint8_t)s.touchMode);
  p.putUChar("kmode", (uint8_t)s.keyboardMode);
  p.putUChar("speed", s.pointerSpeed);
  p.putBool("natural", s.naturalScroll);
  p.putBool("tap", s.tapToClick);
  p.putUChar("led", s.ledBrightness);
  p.putUChar("ver", 2);
  p.putBool("wifi", s.wifi);
  p.putBool("web", s.webEditor);
  p.putUChar("bright", s.screenBrightness);
  p.putUChar("bleid", s.bleIdentity);
  p.end();
}

const char *linkName(Link l) {
  switch (l) {
    case Link::Usb: return "USB";
    case Link::Ble: return "Bluetooth";
    case Link::Both: return "USB + Bluetooth";
  }
  return "?";
}

const char *modeName(Mode m) {
  switch (m) {
    case Mode::Keyboard: return "Keyboard";
    case Mode::KeyboardMouse: return "Kbd+Mouse";
    case Mode::Mouse: return "Mouse";
    case Mode::PadNumpad: return "Pad+Num";
    case Mode::Macros: return "Macros";
    default: return "?";
  }
}

const char *appName(App a) {
  switch (a) {
    case App::Keyboard: return "Keyboard & Mouse";
    case App::Terminal: return "Terminal";
    case App::Calculator: return "Calculator";
    case App::Units: return "Units";
    case App::T48: return "T48 + One ROM";
    case App::Formulas: return "Formulas";
    default: return "?";
  }
}

}  // namespace mt
