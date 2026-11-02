#include "wifi_setup.h"

#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>

#include <algorithm>
#include <vector>

#include "settings.h"

// Release builds leave secrets.h out even when it is there (T5M_NO_SECRETS).
#if __has_include("secrets.h") && !defined(T5M_NO_SECRETS)
#include "secrets.h"
#endif

using namespace t5m;

namespace mt {
namespace {

#ifdef WIFI_SSID
const char *const kDefaultSsid = WIFI_SSID, *const kDefaultPass = WIFI_PASSWORD;
#else
const char *const kDefaultSsid = "", *const kDefaultPass = "";
#endif

constexpr int kW = 1280, kH = 720, kBarH = 64, kKeypadH = 300;
constexpr int kListY = 160, kRowH = 58, kMaxNets = 8;
constexpr uint32_t kWhite = 0xFFFFFF, kBarBg = 0x080A0C, kGood = 0x7CFC9A, kWarn = 0xFFB040;
constexpr uint32_t kJoinTimeoutMs = 20000, kScanTimeoutMs = 15000;

String g_ssid, g_pass;  // the saved network; "" = none

Tab5Keyboard *g_kb = nullptr;
const Theme *g_theme = nullptr;
Keypad g_keypad;
KeypadRenderer g_keypadDraw;
const KeyPage *const kPages[] = {&pages::Qwerty, &pages::Function};
bool g_kbPresent = false;

struct Net {
  String ssid;
  int rssi;
  bool secure;
};
std::vector<Net> g_nets;  // strongest first, one per name
bool g_scanning = false;
uint32_t g_scanStarted = 0;
// A scan can't start while a join is under way: it is tried again every
// couple of seconds until this long after the first try.
uint32_t g_scanFirstTry = 0, g_scanRetryAt = 0;
constexpr uint32_t kScanRetryMs = 2000, kScanGiveUpMs = 20000;

// Typing a network's name (Other network) or its password.
enum class Entry : uint8_t { None, Name, Password };
Entry g_entry = Entry::None;
String g_newSsid, g_text;
bool g_show = false;  // the password in clear

uint32_t g_joinStarted = 0;  // when the last join began; 0 = none since opening
String g_note;               // one line about the last thing done

struct Box {
  int x, y, w, h;
  bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};
const Box kHomeBtn{8, 6, 96, kBarH - 12};
const Box kPowerBtn{kW - 24 - 190, 84, 190, 56};
const Box kForgetBtn{kPowerBtn.x - 16 - 150, 84, 150, 56};
const Box kScanBtn{kForgetBtn.x - 16 - 150, 84, 150, 56};
const Box kField{24, 230, kW - 48, 70};
const Box kCancelBtn{24, 320, 200, 60};
const Box kShowBtn{240, 320, 200, 60};
const Box kJoinBtn{kW - 24 - 200, 320, 200, 60};

Box rowBox(int i) { return {24, kListY + i * kRowH, kW - 48, kRowH - 8}; }

void button(const Box &b, const char *label, bool lit = false) {
  auto &d = M5.Display;
  d.fillRoundRect(b.x, b.y, b.w, b.h, 10, lit ? g_theme->keyLit : g_theme->key);
  d.setFont(&fonts::DejaVu24);
  d.setTextDatum(textdatum_t::middle_center);
  d.setTextColor(lit ? kWhite : g_theme->text);
  d.drawString(label, b.x + b.w / 2, b.y + b.h / 2);
}

bool connected() { return WiFi.status() == WL_CONNECTED; }

// ------------------------------------------------------------------ drawing --

void statusText(String *s, uint32_t *color) {
  *color = g_theme->textDim;
  if (!g_settings.wifi) {
    *s = "Wi-Fi is off";
  } else if (connected()) {
    *s = "Connected to " + WiFi.SSID() + "   " + WiFi.localIP().toString();
    *color = kGood;
  } else if (!*wifiSsid()) {
    *s = "No network yet: pick one below";
  } else if (g_joinStarted && millis() - g_joinStarted < kJoinTimeoutMs) {
    *s = String("Joining ") + wifiSsid() + "...";
  } else {
    const wl_status_t st = WiFi.status();
    if (st == WL_NO_SSID_AVAIL) *s = String("Can't find ") + wifiSsid();
    else if (g_joinStarted) *s = String("Couldn't join ") + wifiSsid() + ": check the name and password";
    else *s = String("Not connected to ") + wifiSsid();
    *color = kWarn;
  }
}

void drawStatus() {
  auto &d = M5.Display;
  String s;
  uint32_t color;
  statusText(&s, &color);
  d.fillRect(0, kBarH + 10, kScanBtn.x - 8, 136 - kBarH, g_theme->background);
  // Long names and messages drop to the smaller font to clear the buttons.
  const int room = kScanBtn.x - 24 - 16;
  d.setFont(d.textWidth(s.c_str(), &fonts::DejaVu24) <= room ? &fonts::DejaVu24 : &fonts::DejaVu18);
  d.setTextDatum(textdatum_t::middle_left);
  d.setTextColor(color);
  d.drawString(s.c_str(), 24, 112);
  if (g_note.length()) {
    d.setFont(&fonts::DejaVu18);
    d.setTextColor(g_theme->textDim);
    d.drawString(g_note.c_str(), 24, 142);
  }
}

void drawBars(int x, int y, int rssi) {
  const int level = rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : 1;
  for (int i = 0; i < 4; i++) {
    const int h = 8 + i * 7;
    M5.Display.fillRect(x + i * 11, y - h, 8, h, i < level ? g_theme->text : g_theme->keyMod);
  }
}

void drawList() {
  auto &d = M5.Display;
  d.fillRect(0, kListY - 4, kW, kH - kListY + 4, g_theme->background);
  d.setFont(&fonts::DejaVu24);
  if (!g_settings.wifi) return;
  const int n = std::min((int)g_nets.size(), kMaxNets);
  for (int i = 0; i < n; i++) {
    const Box b = rowBox(i);
    const Net &net = g_nets[i];
    d.fillRoundRect(b.x, b.y, b.w, b.h, 10, g_theme->key);
    d.setTextDatum(textdatum_t::middle_left);
    d.setTextColor(g_theme->text);
    d.drawString(net.ssid.c_str(), b.x + 24, b.y + b.h / 2);
    const bool here = connected() && WiFi.SSID() == net.ssid;
    const char *tag = here ? "connected" : net.ssid == wifiSsid() ? "saved" : net.secure ? "" : "open";
    d.setTextDatum(textdatum_t::middle_right);
    d.setTextColor(here ? kGood : g_theme->textDim);
    d.drawString(tag, b.x + b.w - 90, b.y + b.h / 2);
    drawBars(b.x + b.w - 66, b.y + b.h / 2 + 15, net.rssi);
  }
  const Box other = rowBox(n);
  d.fillRoundRect(other.x, other.y, other.w, other.h, 10, g_theme->keyMod);
  d.setTextDatum(textdatum_t::middle_left);
  d.setTextColor(g_theme->text);
  d.drawString(n ? "Other network (type its name)" : g_scanning ? "Scanning..." : "Other network (type its name)",
               other.x + 24, other.y + other.h / 2);
}

void drawField() {
  auto &d = M5.Display;
  d.fillRoundRect(kField.x, kField.y, kField.w, kField.h, 10, g_theme->pad);
  d.drawRoundRect(kField.x, kField.y, kField.w, kField.h, 10, g_theme->padEdge);
  String shown = g_text;
  if (g_entry == Entry::Password && !g_show)
    for (unsigned i = 0; i < shown.length(); i++) shown.setCharAt(i, '*');
  shown += "_";
  d.setFont(&fonts::DejaVu40);
  d.setTextDatum(textdatum_t::middle_left);
  d.setTextColor(g_theme->text);
  d.drawString(shown.c_str(), kField.x + 20, kField.y + kField.h / 2);
}

void drawEntry() {
  auto &d = M5.Display;
  d.fillRect(0, kListY - 4, kW, kH - kListY + 4, g_theme->background);
  d.setFont(&fonts::DejaVu24);
  d.setTextDatum(textdatum_t::middle_left);
  d.setTextColor(g_theme->text);
  const String label = g_entry == Entry::Name ? String("Network name") : "Password for " + g_newSsid;
  d.drawString(label.c_str(), 24, 200);
  drawField();
  button(kCancelBtn, "Cancel");
  if (g_entry == Entry::Password) button(kShowBtn, g_show ? "Hide" : "Show");
  button(kJoinBtn, g_entry == Entry::Name ? "Next" : "Join", true);
  if (g_kbPresent) {
    d.setFont(&fonts::DejaVu18);
    d.setTextDatum(textdatum_t::middle_left);
    d.setTextColor(g_theme->textDim);
    d.drawString(g_entry == Entry::Name ? "Enter goes on to the password, Esc cancels"
                                        : "Enter joins, Esc cancels. Leave it empty for an open network.",
                 24, 410);
  } else {
    g_keypad.setRect({0, kH - kKeypadH, kW, kKeypadH});
    g_keypadDraw.invalidate();
  }
}

void drawAll() {
  auto &d = M5.Display;
  d.startWrite();
  d.fillScreen(g_theme->background);
  d.fillRect(0, 0, kW, kBarH, kBarBg);
  button(kHomeBtn, "Home");
  d.setFont(&fonts::DejaVu24);
  d.setTextDatum(textdatum_t::middle_left);
  d.setTextColor(g_theme->text);
  d.drawString("Wi-Fi", 124, kBarH / 2);
  button(kScanBtn, g_scanning ? "..." : "Scan");
  button(kForgetBtn, "Forget");
  button(kPowerBtn, g_settings.wifi ? "Wi-Fi: on" : "Wi-Fi: off", g_settings.wifi);
  drawStatus();
  g_keypad.setRect({});
  if (g_entry == Entry::None) drawList();
  else drawEntry();
  d.endWrite();
}

// ------------------------------------------------------------------ actions --

void save() {
  Preferences p;
  if (!p.begin("wifi", false)) return;
  p.putString("ssid", g_ssid);
  p.putString("pass", g_pass);
  p.end();
}

// One try at starting a scan; a failure is retried from scanTick().
void tryScan() {
  g_scanRetryAt = 0;
  if (WiFi.getMode() == WIFI_OFF) WiFi.mode(WIFI_STA);
  WiFi.scanDelete();
  if (WiFi.scanNetworks(true) == WIFI_SCAN_FAILED) {
    g_scanRetryAt = millis() + kScanRetryMs;
    return;
  }
  g_scanStarted = millis();
}

void startScan() {
  if (!g_settings.wifi || g_scanning) return;
  g_scanning = true;
  g_scanFirstTry = millis();
  tryScan();
}

// A failed start or scan: try again, or give up after a while.
bool retryScan() {
  if (millis() - g_scanFirstTry < kScanGiveUpMs) {
    g_scanRetryAt = millis() + kScanRetryMs;
    return true;
  }
  g_scanning = false;
  g_scanRetryAt = 0;
  g_note = "Scan failed: tap Scan to try again";
  if (g_entry == Entry::None) drawAll();
  return false;
}

void scanTick() {
  if (!g_scanning) return;
  if (g_scanRetryAt) {
    if ((int32_t)(millis() - g_scanRetryAt) >= 0) tryScan();
    if (g_scanRetryAt && millis() - g_scanFirstTry >= kScanGiveUpMs) retryScan();
    return;
  }
  const int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING && millis() - g_scanStarted < kScanTimeoutMs) return;
  if (n < 0) {
    retryScan();
    return;
  }
  g_scanning = false;
  g_nets.clear();
  for (int i = 0; i < n; i++) {
    const String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;  // hidden networks: Other network
    const int rssi = WiFi.RSSI(i);
    auto same = std::find_if(g_nets.begin(), g_nets.end(), [&](const Net &x) { return x.ssid == ssid; });
    if (same != g_nets.end()) {
      same->rssi = std::max(same->rssi, rssi);
      continue;
    }
    g_nets.push_back({ssid, rssi, WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
  }
  std::sort(g_nets.begin(), g_nets.end(), [](const Net &a, const Net &b) { return a.rssi > b.rssi; });
  g_note = "";
  WiFi.scanDelete();
  if (g_entry == Entry::None) drawAll();
}

void join(const String &ssid, const String &pass) {
  g_ssid = ssid;
  g_pass = pass;
  save();
  if (!g_settings.wifi) {
    g_settings.wifi = true;
    settingsChanged();
  }
  g_entry = Entry::None;
  g_note = "";
  WiFi.disconnect();
  wifiConnect();
  g_joinStarted = millis();
  if (!g_joinStarted) g_joinStarted = 1;
  drawAll();
}

void beginEntry(Entry e, const String &ssid) {
  g_entry = e;
  g_newSsid = ssid;
  // The saved network's password comes back, so Enter just rejoins.
  g_text = e == Entry::Password && ssid == g_ssid ? g_pass : String();
  g_show = false;
  drawAll();
}

void finishEntry() {
  if (g_entry == Entry::Name) {
    if (g_text.length()) beginEntry(Entry::Password, g_text);
    return;
  }
  if (g_text.length() && g_text.length() < 8) {
    g_note = "Wi-Fi passwords are 8 characters or more";
    drawStatus();
    return;
  }
  join(g_newSsid, g_text);
}

void forget() {
  if (!g_ssid.length()) {
    g_note = *kDefaultSsid ? "Nothing saved: this build's own network is in use" : "No network saved";
    drawStatus();
    return;
  }
  g_note = "Forgot " + g_ssid;
  g_ssid = g_pass = "";
  save();
  WiFi.disconnect();
  wifiConnect();  // a development build's own network, if it has one
  g_joinStarted = 0;
  drawAll();
}

void power() {
  g_settings.wifi = !g_settings.wifi;
  settingsChanged();
  if (g_settings.wifi) {
    wifiConnect();
    startScan();
  } else {
    g_scanning = false;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    g_entry = Entry::None;
  }
  g_note = "";
  drawAll();
}

void typed(const KeyText &t) {
  if (g_entry == Entry::None) {
    if (t.special == key::Escape) appGoHome();
    return;
  }
  const size_t cap = g_entry == Entry::Name ? 32 : 63;
  if (t.special == key::Enter || t.special == key::KpEnter) {
    finishEntry();
  } else if (t.special == key::Escape) {
    g_entry = Entry::None;
    drawAll();
  } else if (t.special == key::Backspace) {
    if (g_text.length()) {
      g_text.remove(g_text.length() - 1);
      drawField();
    }
  } else if (t.ch >= ' ' && t.ch <= '~' && !t.ctrl && g_text.length() < cap) {
    g_text += t.ch;
    drawField();
  }
}

void tap(int x, int y) {
  if (kHomeBtn.contains(x, y)) { appGoHome(); return; }
  if (kPowerBtn.contains(x, y)) { power(); return; }
  if (kForgetBtn.contains(x, y)) { forget(); return; }
  if (kScanBtn.contains(x, y)) { startScan(); drawAll(); return; }
  if (g_entry != Entry::None) {
    if (kCancelBtn.contains(x, y)) {
      g_entry = Entry::None;
      drawAll();
    } else if (kJoinBtn.contains(x, y)) {
      finishEntry();
    } else if (g_entry == Entry::Password && kShowBtn.contains(x, y)) {
      g_show = !g_show;
      button(kShowBtn, g_show ? "Hide" : "Show");
      drawField();
    }
    return;
  }
  if (!g_settings.wifi) return;
  const int n = std::min((int)g_nets.size(), kMaxNets);
  for (int i = 0; i <= n; i++) {
    if (!rowBox(i).contains(x, y)) continue;
    if (i == n) beginEntry(Entry::Name, "");
    else if (!g_nets[i].secure) join(g_nets[i].ssid, "");
    else beginEntry(Entry::Password, g_nets[i].ssid);
    return;
  }
}

}  // namespace

// ------------------------------------------------------------------ network --

void wifiLoad() {
  Preferences p;
  if (!p.begin("wifi", true)) return;
  g_ssid = p.getString("ssid", "");
  g_pass = p.getString("pass", "");
  p.end();
}

const char *wifiSsid() { return g_ssid.length() ? g_ssid.c_str() : kDefaultSsid; }
const char *wifiPass() { return g_ssid.length() ? g_pass.c_str() : kDefaultPass; }

void wifiConnect() {
  if (!g_settings.wifi || !*wifiSsid()) return;
  WiFi.mode(WIFI_STA);
  // The default fast scan joins the first AP heard, not the strongest.
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifiSsid(), wifiPass());
}

// ------------------------------------------------------------------- screen --

void wifiScreenBegin(Tab5Keyboard &kb, const Theme &theme) {
  g_kb = &kb;
  g_theme = &theme;
  g_keypad.setPages(kPages, 2);
  g_kbPresent = kb.present();
  g_entry = Entry::None;
  g_note = "";
  g_joinStarted = 0;
  startScan();
  drawAll();
}

void wifiScreenLoop(uint32_t now) {
  if (g_kb->present() != g_kbPresent) {
    g_kbPresent = g_kb->present();
    drawAll();  // the on-screen keyboard comes or goes
  }
  TouchPoint touches[kMaxTouches];
  const int n = readM5Touch(touches);
  const bool pad = g_keypad.rect().w > 0;
  if (pad) g_keypad.handle(touches, n);
  for (int i = 0; i < M5.Touch.getCount(); i++) {
    const auto &t = M5.Touch.getDetail(i);
    if (t.wasPressed() && !(pad && t.y >= g_keypad.rect().y)) tap(t.x, t.y);
  }
  KeyEvent e;
  KeyText kt;
  while (g_kb->next(&e))
    if (KeyText::from(e, &kt)) typed(kt);
  while (g_keypad.next(&e))
    if (KeyText::from(e, &kt)) typed(kt);
  if (g_keypad.rect().w > 0) g_keypadDraw.draw(M5.Display, g_keypad, *g_theme);
  scanTick();

  // The status line follows the connection.
  static uint32_t last = 0;
  static String shown;
  if (now - last > 500) {
    last = now;
    String s;
    uint32_t color;
    statusText(&s, &color);
    if (s != shown) {
      const bool wasUp = shown.startsWith("Connected");
      shown = s;
      if (g_entry == Entry::None && wasUp != s.startsWith("Connected")) drawAll();  // the list's tags too
      else drawStatus();
    }
  }
}

void wifiScreenType(char c) {
  KeyText t;
  if (c == '\n' || c == '\r') t.special = key::Enter;
  else if (c == 27) t.special = key::Escape;
  else if (c == 8 || c == 127) t.special = key::Backspace;
  else t.ch = c;
  typed(t);
}

void wifiScreenTap(int x, int y) { tap(x, y); }

}  // namespace mt
