// Saved settings (NVS). Written from loop() only: on the P4 a task with its
// stack in PSRAM may not touch flash.
#pragma once

#include <cstdint>

namespace mt {

enum class Link : uint8_t { Usb, Ble, Both };
enum class Mode : uint8_t { Keyboard, KeyboardMouse, Mouse, PadNumpad, Macros, kCount };
// Apps on the home screen. The one in use is remembered and opens at boot.
enum class App : uint8_t { Keyboard, Terminal, Calculator, Units, T48, Formulas, kCount };

struct Settings {
  App app = App::Keyboard;
  Link link = Link::Usb;
  Mode touchMode = Mode::KeyboardMouse;  // last mode used without the clip-on keyboard
  Mode keyboardMode = Mode::PadNumpad;   // ... and with it
  uint8_t pointerSpeed = 5;              // 1..10
  bool naturalScroll = true;
  bool tapToClick = true;
  uint8_t ledBrightness = 3;             // keyboard status LEDs, 0..100
  bool wifi = true;
  bool webEditor = true;                 // macro editor at http://<ip>/
  uint8_t screenBrightness = 160;
  uint8_t bleIdentity = 1;  // see BleHidConfig::identity; Forget steps it
};

extern Settings g_settings;
void loadSettings();
// False on a first boot: nothing has been saved yet.
bool settingsFound();
// Marks the settings changed; they are saved a few seconds later from loop().
void settingsChanged();
void settingsTick(uint32_t now);

const char *linkName(Link l);
const char *modeName(Mode m);
const char *appName(App a);

}  // namespace mt
