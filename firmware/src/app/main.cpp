// Tab5 Multitool: a home screen of apps.
//
// Apps:
//   Keyboard & Mouse   the Tab5 as a USB / Bluetooth keyboard + mouse (below)
//   Terminal           SwiftTerm over SSH or a USB serial adapter (terminal.cpp)
//   Calculator         RPN scientific calculator (calc_app.cpp, rpn.cpp)
//   Units              unit conversion, 24 kinds (units_app.cpp, units.cpp)
//   T48 + One ROM      T48 for Tab5, the ROM programmer (GPL-3, lib/t48)
// The last app used opens at boot. The Terminal, Calculator and Units come
// and go without a restart; Keyboard & Mouse (USB-C HID, Bluetooth) and T48 (which brings its
// own USB host, SD card, Wi-Fi and web server) start from a clean boot, and
// Home from them restarts the Tab5 to the home screen.
//
// Keyboard & Mouse screen: a status bar (mode buttons, link + lock indicators, battery,
// settings) over the body, which depends on the mode:
//   Keyboard    full-screen on-screen keyboard
//   Kbd+Mouse   trackpad above, keyboard below
//   Mouse       full-screen trackpad
//   Pad+Num     trackpad + numpad/nav (the default with the clip-on keyboard,
//               whose keys go straight to the host)
//   Macros      a macropad: pages of buttons from /macros.json, edited in a
//               browser at the Tab5's address (see web.cpp)
// Sym+Esc on the clip-on keyboard steps through the modes.
//
// Keyboard LEDs (dim; brightness in settings): left = Wi-Fi (green joined,
// amber blinking while joining), right = Bluetooth (blue connected, blinking
// while waiting for a host) - or USB (white) when Bluetooth is off.
//
// Console (USB CDC port while USB HID runs, else USB-Serial-JTAG):
//   s status   m next mode   1-5 pick a mode   j restart into USB-Serial-JTAG
//   P screenshot (raw RGB565 between "SHOT w h" and "END"; tools/shot.py)
//   > next macro page   S open/close Setup   h home
//   k / t / c / u / r   open Keyboard / Terminal / Calculator / Units / T48 from home
// Calculator, Units: i<line> types the line (its end as Enter), @x,y<newline> taps.
// In the T48 app the console is T48 for Tab5's own (its README).
#include <M5Unified.h>
#include <Tab5Multitool.h>
#include <WiFi.h>
#include <esp_attr.h>

#include "burner.h"
#include "calc_app.h"
#include "host.h"
#include "macros.h"
#include "settings.h"
#include "terminal.h"
#include "units_app.h"
#include "web.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

using namespace t5m;

namespace mt {
namespace {

constexpr int kW = 1280, kH = 720, kBarH = 64;
// RGB888 colours; as uint32_t so LovyanGFX doesn't take them as RGB565.
constexpr uint32_t kBarBg = 0x080A0C, kIndOn = 0x7CFC9A, kIndOff = 0x5A646E, kWarn = 0xFFB040, kWhite = 0xFFFFFF;
constexpr uint32_t kStayJtagMagic = 0x4A544147;  // "JTAG"
RTC_NOINIT_ATTR uint32_t g_stay_jtag;
constexpr uint32_t kGoHomeMagic = 0x484F4D45;  // "HOME"
RTC_NOINIT_ATTR uint32_t g_go_home;
// Crashes in a row before the app got going. Two of them and this boot uses
// USB alone, so a transport that panics at start can't loop the Tab5 forever.
RTC_NOINIT_ATTR uint32_t g_boot_crashes;
constexpr uint32_t kSettledMs = 15000;
bool g_safeBoot = false;

UsbHid g_usb;
BleHid g_ble;
HidFanout g_out(nullptr, nullptr);
Tab5Keyboard g_kb;
Keypad g_keypad;
Trackpad g_pad(g_out);
KeypadRenderer g_keypadDraw;
MacroPad g_macroPad;
MacroPadRenderer g_macroDraw;
MacroRunner g_runner(g_out);
uint32_t g_macroVersion = 0;
TrackpadRenderer g_padDraw;
Theme g_theme;

// What this boot is running: the home screen or one app.
enum class Running : uint8_t { Home, Keyboard, Terminal, Calculator, Units, T48 };
Running g_running = Running::Home;

bool g_jtagOnly = false;  // this boot keeps USB-C on USB-Serial-JTAG
bool g_settingsOpen = false;
Mode g_mode = Mode::KeyboardMouse;
bool g_kbWasPresent = false;

const KeyPage *const kFullPages[] = {&pages::Qwerty, &pages::Function, &pages::Numpad, &pages::NavMedia};
const KeyPage *const kSidePages[] = {&pages::Numpad, &pages::NavMedia, &pages::Function};

Print &console() {
  if (g_usb.running()) return g_usb.console();
  return Serial;
}

// Library logs go to whichever console is live when they are written.
struct ConsolePrint : Print {
  size_t write(uint8_t c) override { return console().write(c); }
  size_t write(const uint8_t *b, size_t n) override { return console().write(b, n); }
} g_consolePrint;

// ---------------------------------------------------------------- layout --

void layoutMode(Mode m) {
  g_mode = m;
  Rect none;
  const Rect body{0, kBarH, kW, kH - kBarH};
  g_macroPad.setRect(none);
  switch (m) {
    case Mode::Macros:
      g_macroPad.setRect(body);
      g_keypad.setRect(none);
      g_pad.setRect(none);
      break;
    case Mode::Keyboard:
      g_keypad.setPages(kFullPages, 4);
      g_keypad.setRect(body);
      g_pad.setRect(none);
      break;
    case Mode::KeyboardMouse:
      g_keypad.setPages(kFullPages, 4);
      g_pad.config().buttonStrip = 64;
      g_pad.setRect({0, kBarH, kW, 250});
      g_keypad.setRect({0, kBarH + 250, kW, kH - kBarH - 250});
      break;
    case Mode::Mouse:
      g_pad.config().buttonStrip = 110;
      g_pad.setRect(body);
      g_keypad.setRect(none);
      break;
    case Mode::PadNumpad:
    default:
      g_keypad.setPages(kSidePages, 3);
      g_pad.config().buttonStrip = 96;
      g_pad.setRect({0, kBarH, 820, kH - kBarH});
      g_keypad.setRect({820, kBarH, kW - 820, kH - kBarH});
      break;
  }
  g_keypadDraw.invalidate();
  g_padDraw.invalidate();
  g_macroDraw.invalidate();
  M5.Display.fillRect(0, kBarH, kW, kH - kBarH, g_theme.background);
}

void applyPadSettings() {
  auto &c = g_pad.config();
  c.speed = 0.4f + 0.3f * g_settings.pointerSpeed;
  c.naturalScroll = g_settings.naturalScroll;
  c.tapToClick = g_settings.tapToClick;
}

// Remembers the mode per situation (clip-on keyboard or not).
void chooseMode(Mode m) {
  if (g_kb.present()) g_settings.keyboardMode = m;
  else g_settings.touchMode = m;
  settingsChanged();
  layoutMode(m);
}

void nextMode() {
  chooseMode((Mode)(((uint8_t)g_mode + 1) % (uint8_t)Mode::kCount));
}

// ------------------------------------------------------------ status bar --

const Mode kModeButtons[] = {Mode::Keyboard, Mode::KeyboardMouse, Mode::Mouse, Mode::PadNumpad, Mode::Macros};
constexpr int kModeButtonCount = sizeof(kModeButtons) / sizeof(kModeButtons[0]);
constexpr int kModeBtnW = 136, kModeBtnGap = 8, kSettingsBtnW = 110, kHomeBtnW = 90;

Rect homeButton() { return {8, 6, kHomeBtnW, kBarH - 12}; }
Rect modeButton(int i) {
  return {8 + kHomeBtnW + kModeBtnGap + i * (kModeBtnW + kModeBtnGap), 6, kModeBtnW, kBarH - 12};
}
Rect settingsButton() { return {kW - kSettingsBtnW - 8, 6, kSettingsBtnW, kBarH - 12}; }

void drawButton(const Rect &r, const char *label, bool lit) {
  M5.Display.fillRoundRect(r.x, r.y, r.w, r.h, 10, lit ? g_theme.keyLit : g_theme.key);
  M5.Display.setFont(M5.Display.textWidth(label, &fonts::DejaVu24) < r.w - 12 ? &fonts::DejaVu24 : &fonts::DejaVu18);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.setTextColor(lit ? kWhite : g_theme.text);
  M5.Display.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
}

struct BarState {
  int mode = -1;
  bool settings = false;
  bool usb = false, usbRun = false, ble = false, bleRun = false, wifi = false, kb = false;
  uint8_t leds = 0xFF;
  int battery = -1;
  bool operator==(const BarState &o) const {
    return mode == o.mode && settings == o.settings && usb == o.usb && usbRun == o.usbRun && ble == o.ble &&
           bleRun == o.bleRun && wifi == o.wifi && kb == o.kb && leds == o.leds && battery == o.battery;
  }
};
BarState g_bar;

void drawBar(bool force) {
  BarState s;
  s.mode = (int)g_mode;
  s.settings = g_settingsOpen;
  s.usbRun = g_usb.running();
  s.usb = g_usb.connected();
  s.bleRun = g_ble.running();
  s.ble = g_ble.connected();
  s.wifi = WiFi.status() == WL_CONNECTED;
  s.kb = g_kb.present();
  s.leds = g_out.leds();
  s.battery = M5.Power.getBatteryLevel();
  if (!force && s == g_bar) return;
  g_bar = s;

  auto &d = M5.Display;
  d.startWrite();
  d.fillRect(0, 0, kW, kBarH, kBarBg);
  if (g_settingsOpen) {
    d.setFont(&fonts::DejaVu24);
    d.setTextDatum(textdatum_t::middle_left);
    d.setTextColor(g_theme.text);
    d.drawString("Settings", 24, kBarH / 2);
  } else {
    drawButton(homeButton(), "Home", false);
    for (int i = 0; i < kModeButtonCount; i++)
      drawButton(modeButton(i), modeName(kModeButtons[i]), g_mode == kModeButtons[i]);
  }
  drawButton(settingsButton(), g_settingsOpen ? "Done" : "Setup", g_settingsOpen);

  // Indicators, right to left before the settings button.
  int x = settingsButton().x - 16;
  d.setFont(&fonts::DejaVu18);
  d.setTextDatum(textdatum_t::middle_right);
  const int leftmost = g_settingsOpen ? 200 : modeButton(kModeButtonCount - 1).x + kModeBtnW + 12;
  auto ind = [&](const char *label, bool shown, bool on) {
    if (!shown || x - d.textWidth(label) < leftmost) return;
    d.setTextColor(on ? kIndOn : kIndOff);
    d.drawString(label, x, kBarH / 2);
    x -= d.textWidth(label) + 18;
  };
  char batt[12];
  snprintf(batt, sizeof batt, "%d%%", s.battery);
  ind(batt, s.battery >= 0, true);
  ind("WiFi", g_settings.wifi, s.wifi);
  ind("BT", s.bleRun, s.ble);
  ind(g_jtagOnly ? "USB:serial" : "USB", s.usbRun || g_jtagOnly, s.usb);
  ind("KB", true, s.kb);
  ind("NUM", s.usb || s.ble, s.leds & led::NumLock);
  ind("CAPS", s.usb || s.ble, s.leds & led::CapsLock);
  d.endWrite();
}

// -------------------------------------------------------------- settings --

struct Row {
  const char *label;
  int kind;  // 0 cycle/toggle, 1 minus/plus, 2 action
};
const Row kRows[] = {
    {"Connection (restarts)", 0}, {"Pointer speed", 1},       {"Natural scrolling", 0},
    {"Tap to click", 0},          {"Keyboard LED brightness", 1}, {"Screen brightness", 1},
    {"Wi-Fi", 0},                 {"Macro web editor", 0},
    {"Forget Bluetooth hosts (new identity, restarts)", 2},  {"Firmware update mode (USB serial)", 2},
};
constexpr int kRowCount = sizeof(kRows) / sizeof(kRows[0]);
constexpr int kRowH = 64;
Link g_linkAtOpen = Link::Usb;

void rowValue(int i, char *buf, size_t n) {
  const Settings &s = g_settings;
  switch (i) {
    case 0: snprintf(buf, n, "%s", linkName(s.link)); break;
    case 1: snprintf(buf, n, "%u", s.pointerSpeed); break;
    case 2: snprintf(buf, n, "%s", s.naturalScroll ? "on" : "off"); break;
    case 3: snprintf(buf, n, "%s", s.tapToClick ? "on" : "off"); break;
    case 4: snprintf(buf, n, "%u%%", s.ledBrightness); break;
    case 5: snprintf(buf, n, "%u", s.screenBrightness); break;
    case 6: snprintf(buf, n, "%s", s.wifi ? "on" : "off"); break;
    case 7:
      if (!s.webEditor) snprintf(buf, n, "off");
      else if (webRunning()) snprintf(buf, n, "http://%s/", WiFi.localIP().toString().c_str());
      else snprintf(buf, n, "on (waiting for Wi-Fi)");
      break;
    default: buf[0] = 0; break;
  }
}

void drawSettings() {
  auto &d = M5.Display;
  d.startWrite();
  d.fillRect(0, kBarH, kW, kH - kBarH, g_theme.background);
  for (int i = 0; i < kRowCount; i++) {
    const int y = kBarH + 10 + i * kRowH;
    d.fillRoundRect(20, y, kW - 40, kRowH - 8, 10, g_theme.key);
    d.setFont(&fonts::DejaVu24);
    d.setTextColor(g_theme.text);
    d.setTextDatum(textdatum_t::middle_left);
    d.drawString(kRows[i].label, 40, y + (kRowH - 8) / 2);
    char v[48];
    rowValue(i, v, sizeof v);
    d.setTextDatum(textdatum_t::middle_right);
    if (kRows[i].kind == 1) {
      d.drawString("-", kW - 300, y + (kRowH - 8) / 2);
      d.drawString(v, kW - 160, y + (kRowH - 8) / 2);
      d.drawString("+", kW - 50, y + (kRowH - 8) / 2);
    } else {
      d.drawString(v, kW - 50, y + (kRowH - 8) / 2);
    }
  }
  d.endWrite();
}

void restartToJtag() {
  g_stay_jtag = kStayJtagMagic;
  console().println("restarting into USB-Serial-JTAG");
  delay(100);
  esp_restart();
}

void drawHome();
void restartHome();
bool g_homeRequested = false;  // set by Home during an app's loop pass
bool g_otherAppRan = false;    // another app used USB-A or the radio this boot

// Back to the home screen. The Terminal stops itself; the Keyboard app
// (USB-C HID, Bluetooth) needs a restart to let go cleanly.
void goHome() {
  if (g_running == Running::Home) return;
  if (g_running == Running::Terminal || g_running == Running::Calculator || g_running == Running::Units) {
    if (g_running == Running::Terminal) terminalEnd();
    else if (g_running == Running::Calculator) calcEnd();
    else unitsEnd();
    g_running = Running::Home;
    drawHome();
    return;
  }
  settingsTick(millis() + 100000);  // save anything pending first
  restartHome();
}

void restartHome() {
  g_go_home = kGoHomeMagic;
  delay(50);
  esp_restart();
}

void settingsTap(int x, int y) {
  const int i = (y - kBarH - 10) / kRowH;
  if (i < 0 || i >= kRowCount) return;
  Settings &s = g_settings;
  const bool minus = x < kW - 230;
  auto adj = [&](uint8_t &v, int step, int lo, int hi) {
    int n = v + (minus ? -step : step);
    v = (uint8_t)(n < lo ? lo : n > hi ? hi : n);
  };
  switch (i) {
    case 0: s.link = (Link)(((uint8_t)s.link + 1) % 3); break;
    case 1: adj(s.pointerSpeed, 1, 1, 10); applyPadSettings(); break;
    case 2: s.naturalScroll = !s.naturalScroll; applyPadSettings(); break;
    case 3: s.tapToClick = !s.tapToClick; applyPadSettings(); break;
    case 4: adj(s.ledBrightness, s.ledBrightness < 10 ? 1 : 5, 0, 100); g_kb.setLedBrightness(s.ledBrightness); break;
    case 5: adj(s.screenBrightness, 16, 16, 255); M5.Display.setBrightness(s.screenBrightness); break;
    case 6: s.wifi = !s.wifi; break;
    case 7: s.webEditor = !s.webEditor; if (s.webEditor) webBegin(); break;
    case 8:
      // Forget hosts here, and become a new device to them (see BleHidConfig::identity).
      if (g_ble.running()) g_ble.forgetHosts();
      s.bleIdentity = s.bleIdentity == 255 ? 1 : s.bleIdentity + 1;
      settingsChanged();
      settingsTick(millis() + 100000);
      delay(50);
      esp_restart();
      break;
    case 9: restartToJtag(); break;
  }
  settingsChanged();
  drawSettings();
}

void openSettings(bool open) {
  g_settingsOpen = open;
  if (open) {
    g_linkAtOpen = g_settings.link;
    drawSettings();
  } else {
    if (g_settings.link != g_linkAtOpen) {
      // Transports are chosen at boot: save now and restart.
      settingsChanged();
      settingsTick(millis() + 100000);
      delay(50);
      esp_restart();
    }
    layoutMode(g_mode);
  }
  drawBar(true);
}

// ----------------------------------------------------------------- input --

TouchPoint g_touch[kMaxTouches];
int g_touchCount = 0;
bool g_barFinger[16] = {};     // touch ids that started on the status bar
bool g_widgetFinger[16] = {};  // ... and below it (may slide up over the bar)

void handleTouch(uint32_t now) {
  g_touchCount = readM5Touch(g_touch);
  // Taps that start on the status bar (or anywhere in settings) are buttons.
  for (int i = 0; i < M5.Touch.getCount(); i++) {
    const auto &t = M5.Touch.getDetail(i);
    if (!t.wasPressed()) continue;
    if (t.y < kBarH) {
      if (settingsButton().contains(t.x, t.y)) openSettings(!g_settingsOpen);
      else if (!g_settingsOpen && homeButton().contains(t.x, t.y)) goHome();
      else if (!g_settingsOpen)
        for (int b = 0; b < kModeButtonCount; b++)
          if (modeButton(b).contains(t.x, t.y)) chooseMode(kModeButtons[b]);
    } else if (g_settingsOpen) {
      settingsTap(t.x, t.y);
    }
  }
  if (g_settingsOpen) return;
  // Fingers that started on the bar never reach the widgets.
  TouchPoint body[kMaxTouches];
  int n = 0;
  for (int i = 0; i < g_touchCount; i++) {
    const int id = g_touch[i].id & 15;
    // A finger that lands on the bar stays a bar finger until it lifts.
    if (g_touch[i].y < kBarH && !g_widgetFinger[id]) g_barFinger[id] = true;
    if (!g_barFinger[id]) g_widgetFinger[id] = true;
    if (!g_barFinger[id]) body[n++] = g_touch[i];
  }
  for (int id = 0; id < 16; id++) {
    if (!g_barFinger[id]) continue;
    bool down = false;
    for (int i = 0; i < g_touchCount; i++) if ((g_touch[i].id & 15) == id) down = true;
    if (!down) g_barFinger[id] = false;
  }
  for (int id = 0; id < 16; id++) {
    if (!g_widgetFinger[id]) continue;
    bool down = false;
    for (int i = 0; i < g_touchCount; i++) if ((g_touch[i].id & 15) == id) down = true;
    if (!down) g_widgetFinger[id] = false;
  }
  if (g_keypad.rect().w > 0) g_keypad.handle(body, n);
  if (g_pad.rect().w > 0) g_pad.handle(body, n, now);
  if (g_macroPad.rect().w > 0) g_macroPad.handle(body, n);
}

void pumpKeys() {
  KeyEvent e;
  while (g_kb.next(&e)) {
    if (e.usage == Tab5Keyboard::kAppKey) {
      if (e.down) nextMode();
      continue;
    }
    g_out.apply(e);
  }
  while (g_keypad.next(&e)) g_out.apply(e);
  uint16_t media;
  while (g_keypad.nextMedia(&media)) g_out.consumer(media);
  g_keypad.setHostLeds(g_out.leds());
  const MacroButton *b;
  while (g_macroPad.next(&b)) g_runner.run(*b);
}

// ------------------------------------------------------------- LEDs, net --

void updateLeds(uint32_t now) {
  const bool blink = (now / 500) & 1;
  if (!g_settings.wifi) g_kb.setLed(0, 0, 0, 0);
  else if (WiFi.status() == WL_CONNECTED) g_kb.setLed(0, 0, 255, 0);
  else g_kb.setLed(0, blink ? 255 : 0, blink ? 120 : 0, 0);

  if (g_ble.running()) {
    if (g_ble.connected()) g_kb.setLed(1, 0, 0, 255);
    else g_kb.setLed(1, 0, 0, blink ? 255 : 0);
  } else if (g_usb.connected()) {
    g_kb.setLed(1, 255, 255, 255);
  } else {
    g_kb.setLed(1, 0, 0, 0);
  }
}

void wifiBegin() {
  if (!g_settings.wifi) return;
#ifdef WIFI_SSID
  WiFi.mode(WIFI_STA);
  // The default fast scan joins the first AP heard, not the strongest.
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#endif
}

void status(Print &out) {
  out.printf("multitool: mode %s, link %s, keyboard %s (fw %u)\n", modeName(g_mode), linkName(g_settings.link),
             g_kb.present() ? "present" : "absent", g_kb.firmware());
  out.printf("usb: running %d mounted %d suspended %d boot %d | ble: running %d connected %d | leds %02X\n",
             g_usb.running(), g_usb.mounted(), g_usb.suspended(), g_usb.bootProtocol(), g_ble.running(),
             g_ble.connected(), g_out.leds());
  if (g_safeBoot) out.println("SAFE BOOT: crashed twice at startup, Bluetooth skipped");
  out.printf("macros: %u pages, editor %s\n", (unsigned)macros().pages.size(),
             webRunning() ? WiFi.localIP().toString().c_str() : "off");
  if (g_ble.running()) out.printf("ble: address %s, identity %u\n", g_ble.address().c_str(), g_settings.bleIdentity);
  out.printf("wifi: %s %s | battery %d%% | heap %u psram %u\n", WiFi.status() == WL_CONNECTED ? "up" : "down",
             WiFi.localIP().toString().c_str(), (int)M5.Power.getBatteryLevel(), (unsigned)ESP.getFreeHeap(),
             (unsigned)ESP.getFreePsram());
}

// The screen as raw RGB565 (big-endian), row by row, for tools/shot.py.
void screenshot(Print &out) {
  auto &d = M5.Display;
  const int w = d.width(), h = d.height();
  out.printf("\nSHOT %d %d\n", w, h);
  if (&out == &Serial) Serial.setTxTimeoutMs(2000);  // block rather than drop bytes
  static uint16_t row[1280];
  for (int y = 0; y < h; y++) {
    d.readRect(0, y, w, 1, row);  // LovyanGFX hands back swapped (big-endian) RGB565
    out.write((const uint8_t *)row, w * 2);
    if ((y & 15) == 15) out.flush();
  }
  out.print("\nEND\n");
  if (&out == &Serial) Serial.setTxTimeoutMs(0);
}

// ------------------------------------------------------------ home screen --

struct Tile {
  App app;
  const char *blurb[2];
  uint32_t color;
};
const Tile kTiles[] = {
    {App::Keyboard, {"USB + Bluetooth keyboard,", "trackpad and macros"}, 0x2A78E0},
    {App::Terminal, {"SwiftTerm over SSH", "or a USB serial adapter"}, 0x1A9E8A},
    {App::Calculator, {"RPN scientific, with stats,", "registers and HEX/OCT/BIN"}, 0xC77A22},
    {App::Units, {"Length, volume, temperature", "and 21 more kinds"}, 0x7B55C9},
    {App::T48, {"Program ROMs with an", "XGecu T48 or a One ROM"}, 0xB8433A},
};
constexpr int kTileCount = sizeof(kTiles) / sizeof(kTiles[0]);
constexpr int kTileCols = 3;

// Three to a row; a short last row is centred.
Rect tileRect(int i) {
  const int gap = 36, top = kBarH + 36;
  const int rows = (kTileCount + kTileCols - 1) / kTileCols;
  const int w = (kW - gap * (kTileCols + 1)) / kTileCols;
  const int h = (kH - top - gap - (rows - 1) * gap) / rows;
  const int row = i / kTileCols, col = i % kTileCols;
  const int inRow = row == rows - 1 ? kTileCount - row * kTileCols : kTileCols;
  const int x0 = (kW - (inRow * w + (inRow - 1) * gap)) / 2;
  return {x0 + col * (w + gap), top + row * (h + gap), w, h};
}

void drawHome() {
  auto &d = M5.Display;
  d.startWrite();
  d.fillScreen(g_theme.background);
  d.fillRect(0, 0, kW, kBarH, kBarBg);
  d.setFont(&fonts::DejaVu24);
  d.setTextDatum(textdatum_t::middle_left);
  d.setTextColor(g_theme.text);
  d.drawString("Tab5 Multitool", 24, kBarH / 2);
  d.setTextDatum(textdatum_t::middle_right);
  d.setTextColor(g_theme.textDim);
  if (WiFi.status() == WL_CONNECTED) d.drawString(WiFi.localIP().toString().c_str(), kW - 24, kBarH / 2);
  for (int i = 0; i < kTileCount; i++) {
    const Rect r = tileRect(i);
    d.fillRoundRect(r.x, r.y, r.w, r.h, 24, kTiles[i].color);
    d.setTextDatum(textdatum_t::middle_center);
    d.setTextColor(kWhite);
    const char *name = appName(kTiles[i].app);
    d.setFont(d.textWidth(name, &fonts::DejaVu40) < r.w - 24 ? &fonts::DejaVu40 : &fonts::DejaVu24);
    d.drawString(name, r.x + r.w / 2, r.y + r.h / 2 - 34);
    d.setFont(&fonts::DejaVu18);
    d.drawString(kTiles[i].blurb[0], r.x + r.w / 2, r.y + r.h / 2 + 24);
    d.drawString(kTiles[i].blurb[1], r.x + r.w / 2, r.y + r.h / 2 + 50);
  }
  d.endWrite();
}

void startKeyboard();

void startApp(App a) {
  if (g_settings.app != a) {
    g_settings.app = a;
    settingsChanged();
  }
  if (a == App::T48) {
    // The burner sets the Tab5 up its own way from power-on: restart into it.
    settingsTick(millis() + 100000);
    delay(50);
    esp_restart();
  } else if (a == App::Units) {
    g_running = Running::Units;
    unitsBegin(g_kb, g_theme);
  } else if (a == App::Calculator) {
    g_running = Running::Calculator;
    calcBegin(g_kb, g_theme);
  } else if (a == App::Terminal) {
    g_otherAppRan = true;
    g_running = Running::Terminal;
    terminalBegin(g_kb, g_theme);
  } else if (g_otherAppRan) {
    // USB-C HID and Bluetooth start from a clean boot: restart straight into
    // the Keyboard app (the remembered app).
    settingsTick(millis() + 100000);
    delay(50);
    esp_restart();
  } else {
    startKeyboard();
  }
}

void homeLoop() {
  for (int i = 0; i < M5.Touch.getCount(); i++) {
    const auto &t = M5.Touch.getDetail(i);
    if (!t.wasClicked()) continue;
    for (int k = 0; k < kTileCount; k++)
      if (tileRect(k).contains(t.x, t.y)) {
        startApp(kTiles[k].app);
        return;
      }
  }
  // Clip-on keyboard: Enter opens the remembered app.
  KeyEvent e;
  while (g_kb.next(&e))
    if (e.down && e.usage == key::Enter) {
      startApp(g_settings.app);
      return;
    }
  static bool wifi = false;
  if ((WiFi.status() == WL_CONNECTED) != wifi) {
    wifi = !wifi;
    drawHome();
  }
}

extern "C" int t5m_swift_probe(uint8_t *out, int cap);

void command(int ch) {
  Print &out = console();
  // Terminal test hooks: 'f' then a line is shown as if the connection sent
  // it; 'i' then a line is typed (the line's end as Enter). A line being
  // collected takes every character, so typed text can't trigger commands.
  static int line = 0;  // 'f' or 'i' while collecting
  if (g_running == Running::Terminal && (line || ch == 'f' || ch == 'i' || ch == 'P' || ch == 's')) {
    if (line) {
      const uint8_t b = (uint8_t)ch;
      if (line == 'f' && ch == '\n') line = 0;
      else if (line == 'f') terminalFeed(&b, 1);
      else if (ch == '\n') { terminalType("\r"); line = 0; }
      else { const char t[2] = {(char)ch, 0}; terminalType(t); }
    } else if (ch == 'f' || ch == 'i') {
      line = ch;
    } else if (ch == 'P') {
      screenshot(out);
    } else if (ch == 's') {
      out.printf("multitool: terminal | heap %u psram %u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    }
    return;
  }
  if (g_running == Running::Calculator || g_running == Running::Units) {
    const bool calc = g_running == Running::Calculator;
    auto type = [&](char c) { if (calc) calcType(c); else unitsType(c); };
    static int mode = 0;  // 'i' typing a line, '@' collecting tap coordinates
    static char tap[16];
    static int tapLen = 0;
    if (mode == 'i') {
      if (ch == '\n') { type('\n'); mode = 0; }
      else type((char)ch);
    } else if (mode == '@') {
      if (ch == '\n') {
        tap[tapLen] = 0;
        int x = 0, y = 0;
        if (sscanf(tap, "%d,%d", &x, &y) == 2) {
          if (calc) calcTap(x, y);
          else unitsTap(x, y);
        }
        mode = 0;
      } else if (tapLen < 15) {
        tap[tapLen++] = (char)ch;
      }
    } else if (ch == 'i') {
      mode = 'i';
    } else if (ch == '@') {
      mode = '@';
      tapLen = 0;
    } else if (ch == 'P') {
      screenshot(out);
    } else if (ch == 'h') {
      goHome();
    } else if (ch == 's') {
      out.printf("multitool: %s | heap %u psram %u\n", calc ? "calculator" : "units", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    }
    return;
  }
  if (ch == 'w') {
    // Embedded Swift self-test (swift/Sources/T5MSwift/Probe.swift).
    uint8_t buf[160];
    const uint32_t t0 = micros();
    t5m_swift_probe(buf, sizeof buf);
    out.printf("swift: %s (%lu us)\n", (const char *)buf, (unsigned long)(micros() - t0));
    return;
  }
  if (ch == 'h') {
    if (g_running != Running::Home) goHome();
    return;
  }
  if (g_running == Running::Home) {
    if (ch == 'k') startApp(App::Keyboard);
    else if (ch == 't') startApp(App::Terminal);
    else if (ch == 'r') startApp(App::T48);
    else if (ch == 'c') startApp(App::Calculator);
    else if (ch == 'u') startApp(App::Units);
    else if (ch == 'P') screenshot(out);
    else if (ch == 's') out.printf("multitool: home screen, last app %s\n", appName(g_settings.app));
    return;
  }
  if (ch == 's') status(out);
  else if (ch == 'm') nextMode();
  else if (ch >= '1' && ch <= '5') chooseMode(kModeButtons[ch - '1']);
  else if (ch == 'P') screenshot(out);
  else if (ch == '>') g_macroPad.setPage((g_macroPad.page() + 1) % (g_macroPad.pageCount() ? g_macroPad.pageCount() : 1));
  else if (ch == 'S') openSettings(!g_settingsOpen);
  else if (ch == 'j') restartToJtag();
}

// The Keyboard & Mouse app: HID transports, then its screen.
void startKeyboard() {
  g_running = Running::Keyboard;
  M5.Display.fillScreen(g_theme.background);
  const bool wantUsb = (g_settings.link != Link::Ble || g_safeBoot) && !g_jtagOnly;
  const bool wantBle = g_settings.link != Link::Usb && !g_safeBoot;
  if (wantBle) {
    BleHidConfig bc;
    bc.log = &g_consolePrint;
    bc.identity = g_settings.bleIdentity;
    g_ble.begin(bc);
  }
  if (wantUsb) g_usb.begin();
  g_out.set(g_usb.running() ? &g_usb : nullptr, g_ble.running() ? &g_ble : nullptr);

  // Give the keyboard a moment to answer so the first layout is the right one.
  for (int i = 0; i < 10 && !g_kb.present(); i++) { g_kb.tick(); delay(30); }
  g_kbWasPresent = g_kb.present();
  layoutMode(g_kbWasPresent ? g_settings.keyboardMode : g_settings.touchMode);
  drawBar(true);
  if (g_safeBoot) {
    M5.Display.setFont(&fonts::DejaVu24);
    M5.Display.setTextDatum(textdatum_t::bottom_center);
    M5.Display.setTextColor(kWarn);
    M5.Display.drawString("Crashed at startup twice: USB only this boot", kW / 2, kH - 12);
  }
}

void keyboardLoop(uint32_t now) {
  if (g_kb.present() != g_kbWasPresent) {
    g_kbWasPresent = g_kb.present();
    if (!g_settingsOpen) layoutMode(g_kbWasPresent ? g_settings.keyboardMode : g_settings.touchMode);
  }
  handleTouch(now);
  pumpKeys();
  if (!g_settingsOpen) {
    if (g_keypad.rect().w > 0) g_keypadDraw.draw(M5.Display, g_keypad, g_theme);
    if (g_pad.rect().w > 0) g_padDraw.draw(M5.Display, g_pad, g_theme);
    if (g_macroPad.rect().w > 0) g_macroDraw.draw(M5.Display, g_macroPad, g_theme);
  }
  g_runner.tick(now);
  if (macrosVersion() != g_macroVersion) {
    // Saved from the web editor: show the new layout.
    g_macroVersion = macrosVersion();
    g_macroPad.setLayout(&macros());
    g_macroDraw.invalidate();
  }
  static uint32_t lastSlow = 0;
  if (now - lastSlow > 250) {
    lastSlow = now;
    drawBar(false);
    updateLeds(now);
    static uint32_t lastBatt = 0;
    if (g_ble.running() && now - lastBatt > 60000) {
      lastBatt = now;
      g_ble.setBattery(M5.Power.getBatteryLevel());
    }
  }
}

}  // namespace

void appGoHome() { g_homeRequested = true; }

}  // namespace mt

using namespace mt;

// The T48 app: T48 for Tab5's own start-up and loop, with a Home button.
void startT48() {
  g_running = Running::T48;
  burner::host::Config hc;
  hc.home = [] { restartHome(); };
#ifdef WIFI_SSID
  if (g_settings.wifi) {
    hc.wifi_ssid = WIFI_SSID;
    hc.wifi_pass = WIFI_PASSWORD;
  }
#endif
  burner::host::set(hc);
  burner::setup();
}

void setup() {
  // Which app this boot is for comes first: the T48 app does all of its own
  // start-up (M5.begin included).
  loadSettings();
  const bool home = g_go_home == kGoHomeMagic;
  g_go_home = 0;
  const esp_reset_reason_t why = esp_reset_reason();
  if (why == ESP_RST_PANIC || why == ESP_RST_INT_WDT || why == ESP_RST_TASK_WDT || why == ESP_RST_WDT) {
    g_boot_crashes = g_boot_crashes < 100 ? g_boot_crashes + 1 : 100;
  } else {
    g_boot_crashes = 0;
  }
  g_safeBoot = g_boot_crashes >= 2;
  if (!home && !g_safeBoot && g_settings.app == App::T48) {
    startT48();
    return;
  }

  auto cfg = M5.config();
  cfg.output_power = false;  // keep the USB-A rail off: it back-drives USB-C VBUS
  M5.begin(cfg);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  M5.Display.setRotation(3);
  M5.Display.setBrightness(g_settings.screenBrightness);
  M5.Display.fillScreen(g_theme.background);

  M5.Power.setExtOutput(false, m5::ext_USB);  // off, even if the T48 app left it on
  g_jtagOnly = g_stay_jtag == kStayJtagMagic;
  g_stay_jtag = 0;

  macrosBegin();
  g_macroPad.setLayout(&macros());
  g_macroVersion = macrosVersion();
  if (g_settings.webEditor) webBegin();

  g_kb.begin();
  g_kb.setLedBrightness(g_settings.ledBrightness);
  applyPadSettings();

#ifdef T5M_BOOT_JTAG_MS
  // Development builds: stay on USB-Serial-JTAG briefly at every boot, so a
  // power cycle always gives tools/flash.py a way in.
  if (!g_jtagOnly) {
    M5.Display.setFont(&fonts::DejaVu24);
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.setTextColor(g_theme.textDim);
    M5.Display.drawString("Tab5 Multitool", kW / 2, kH / 2);
    const uint32_t until = millis() + T5M_BOOT_JTAG_MS;
    while ((int32_t)(millis() - until) < 0) {
      if (Serial.available() && Serial.read() == 'j') { g_jtagOnly = true; break; }
      delay(10);
    }
  }
#endif
  wifiBegin();
  if (home || (g_safeBoot && g_settings.app == App::T48)) {
    g_running = Running::Home;
    drawHome();
  } else {
    startApp(g_settings.app);
  }
  Serial.println(g_jtagOnly ? "multitool: USB-Serial-JTAG boot (USB HID off)" : "multitool: started");
}

void loop() {
  const uint32_t now = millis();
  if (g_boot_crashes && now > kSettledMs) g_boot_crashes = 0;
  if (g_running == Running::T48) {
    burner::loop();
    return;
  }
  M5.update();
  g_kb.tick();
  switch (g_running) {
    case Running::Home: homeLoop(); break;
    case Running::Keyboard: keyboardLoop(now); break;
    case Running::Terminal: terminalLoop(now); break;
    case Running::Calculator: calcLoop(now); break;
    case Running::Units: unitsLoop(now); break;
    case Running::T48: break;
  }
  if (g_homeRequested) {
    g_homeRequested = false;
    goHome();
  }
  webTick();
  settingsTick(now);

  static bool greeted = false;
  if (g_usb.running() && g_usb.console().open() && !greeted) {
    greeted = true;
    status(g_usb.console());
  }
  if (g_usb.running()) {
    while (g_usb.console().available()) command(g_usb.console().read());
  }
  while (Serial.available()) command(Serial.read());
  delay(2);
}
