// The Terminal app. SwiftTerm (Embedded Swift, swift/) keeps the screen state;
// this file draws it and moves bytes between it and the connection.
//
// Screen: a bar (Home, title, scroll position) over the terminal grid. Without
// the clip-on keyboard, an on-screen keyboard takes the bottom of the screen.
// Drag on the grid to scroll back; any key goes back to the live screen.
//
// Connections: a USB serial adapter on USB-A (usbserial.cpp), SSH over Wi-Fi
// (ssh.cpp), or Loopback (what you type comes back). Setup on the bar picks one.
//
// SSH asks its questions in the terminal, like the ssh command: the host
// (user@host[:port], or the number of a recent one), whether to trust a new
// host key, and the password if this Tab5's key isn't accepted. Enter ~ .
// disconnects. Host keys are remembered, and a changed one is refused.
#include "battery.h"
#include "terminal.h"

#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>

#include <string>

#include "ssh.h"
#include "t5m_term.h"
#include "usbserial.h"

using namespace t5m;

namespace mt {
namespace {

constexpr int kW = 1280, kH = 720, kBarH = 64, kKeypadH = 300;
constexpr int kScrollback = 1000;
constexpr uint32_t kBarBg = 0x080A0C, kDim = 0x8A949E;
const lgfx::IFont *const kFont = &fonts::lgfxJapanGothic_24;

Tab5Keyboard *g_kb = nullptr;
const Theme *g_theme = nullptr;
void *g_term = nullptr;
int g_cols = 0, g_rows = 0, g_cw = 12, g_ch = 24;
int g_top = kBarH;           // grid origin
int g_base = 20;             // the font's baseline in a cell: drawChar draws on it
bool g_kbPresent = false;
bool g_caps = false;
int g_scrolledBack = 0;
bool g_full = true;          // redraw every row next frame
bool g_setupOpen = false;
bool g_connChanged = false;  // Setup switched the connection: greet the new one
void showKey();
void sshBarText(char *buf, size_t n);
void openSetup(bool open);
void setupTap(int x, int y);
LGFX_Sprite g_row(&M5.Display);

Keypad g_keypad;
KeypadRenderer g_keypadDraw;
const KeyPage *const kPages[] = {&pages::Qwerty, &pages::NavMedia, &pages::Function};

Rect homeButton() { return {8, 6, 90, kBarH - 12}; }
Rect setupButton() { return {kW - 118, 6, 110, kBarH - 12}; }

// ------------------------------------------------------------- settings --

enum class Conn : uint8_t { Serial, Ssh, Loopback, kCount };
const uint32_t kBauds[] = {300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
constexpr int kBaudCount = sizeof(kBauds) / sizeof(kBauds[0]);

struct TermSettings {
  Conn conn = Conn::Serial;
  uint8_t baud = 8;      // index into kBauds
  uint8_t dataBits = 8;
  uint8_t parity = 0;    // usbserial::Line
  uint8_t stopBits = 0;
  bool rtscts = false;
  uint8_t enter = 0;     // Enter sends 0 CR, 1 CR+LF, 2 LF
  bool lfToCrlf = false; // incoming LF starts a new line at the left
  bool echo = false;     // show what is sent
} g_set;

void loadTermSettings() {
  Preferences p;
  if (!p.begin("terminal", false)) return;  // read-write: read-only fails until it exists
  g_set.conn = (Conn)p.getUChar("conn", (uint8_t)g_set.conn);
  if (g_set.conn >= Conn::kCount) g_set.conn = Conn::Serial;
  g_set.baud = p.getUChar("baud", g_set.baud) % kBaudCount;
  g_set.dataBits = p.getUChar("bits", g_set.dataBits);
  g_set.parity = p.getUChar("parity", g_set.parity);
  g_set.stopBits = p.getUChar("stop", g_set.stopBits);
  g_set.rtscts = p.getBool("rtscts", g_set.rtscts);
  g_set.enter = p.getUChar("enter", g_set.enter);
  g_set.lfToCrlf = p.getBool("lfcrlf", g_set.lfToCrlf);
  g_set.echo = p.getBool("echo", g_set.echo);
  p.end();
}

void saveTermSettings() {
  Preferences p;
  if (!p.begin("terminal", false)) return;
  p.putUChar("conn", (uint8_t)g_set.conn);
  p.putUChar("baud", g_set.baud);
  p.putUChar("bits", g_set.dataBits);
  p.putUChar("parity", g_set.parity);
  p.putUChar("stop", g_set.stopBits);
  p.putBool("rtscts", g_set.rtscts);
  p.putUChar("enter", g_set.enter);
  p.putBool("lfcrlf", g_set.lfToCrlf);
  p.putBool("echo", g_set.echo);
  p.end();
}

usbserial::Line serialLine() {
  usbserial::Line l;
  l.baud = kBauds[g_set.baud];
  l.dataBits = g_set.dataBits;
  l.parity = g_set.parity;
  l.stopBits = g_set.stopBits;
  l.rtscts = g_set.rtscts;
  return l;
}

// "8N1" style.
void lineName(char *buf, size_t n) {
  static const char kPar[] = "NOEMS";
  snprintf(buf, n, "%lu %u%c%s%s", (unsigned long)kBauds[g_set.baud], g_set.dataBits, kPar[g_set.parity % 5],
           g_set.stopBits == 0 ? "1" : g_set.stopBits == 1 ? "1.5" : "2", g_set.rtscts ? " RTS/CTS" : "");
}

// ---------------------------------------------------------------- drawing --

// Box drawing (U+2500 block) and block elements, drawn as lines and fills:
// the font's glyphs for them are full-width and don't join up.
// Each line entry is the weight toward up, right, down, left:
// 0 none, 1 light, 2 heavy, 3 double.
struct BoxGlyph {
  uint16_t ch;
  uint16_t urdl;  // one hex digit per direction
};
const BoxGlyph kBox[] = {
    {0x2500, 0x0101}, {0x2502, 0x1010}, {0x250C, 0x0110}, {0x2510, 0x0011}, {0x2514, 0x1100},
    {0x2518, 0x1001}, {0x251C, 0x1110}, {0x2524, 0x1011}, {0x252C, 0x0111}, {0x2534, 0x1101},
    {0x253C, 0x1111}, {0x2501, 0x0202}, {0x2503, 0x2020}, {0x250F, 0x0220}, {0x2513, 0x0022},
    {0x2517, 0x2200}, {0x251B, 0x2002}, {0x2523, 0x2220}, {0x252B, 0x2022}, {0x2533, 0x0222},
    {0x253B, 0x2202}, {0x254B, 0x2222}, {0x2550, 0x0303}, {0x2551, 0x3030}, {0x2554, 0x0330},
    {0x2557, 0x0033}, {0x255A, 0x3300}, {0x255D, 0x3003}, {0x2560, 0x3330}, {0x2563, 0x3033},
    {0x2566, 0x0333}, {0x2569, 0x3303}, {0x256C, 0x3333}, {0x256D, 0x0110}, {0x256E, 0x0011},
    {0x256F, 0x1001}, {0x2570, 0x1100}, {0x2504, 0x0101}, {0x2508, 0x0101}, {0x254C, 0x0101},
    {0x2506, 0x1010}, {0x250A, 0x1010}, {0x254E, 0x1010}, {0x2505, 0x0202}, {0x2509, 0x0202},
    {0x254D, 0x0202}, {0x2507, 0x2020}, {0x250B, 0x2020}, {0x254F, 0x2020}, {0x2574, 0x0001},
    {0x2575, 0x1000}, {0x2576, 0x0100}, {0x2577, 0x0010},
};

uint32_t blend(uint32_t a, uint32_t b, int pctA) {
  auto ch = [&](int sh) {
    const int v = (((a >> sh) & 0xFF) * pctA + ((b >> sh) & 0xFF) * (100 - pctA)) / 100;
    return (uint32_t)v << sh;
  };
  return ch(16) | ch(8) | ch(0);
}

// Draws ch at cell x if it is a box or block character; false otherwise.
bool drawBox(uint32_t ch, int x0, uint32_t fg, uint32_t bg) {
  const int w = g_cw, h = g_ch;
  if (ch >= 0x2580 && ch <= 0x259F) {
    if (ch == 0x2580) g_row.fillRect(x0, 0, w, h / 2, fg);
    else if (ch >= 0x2581 && ch <= 0x2588) {  // lower eighths .. full block
      const int hh = h * (int)(ch - 0x2580) / 8;
      g_row.fillRect(x0, h - hh, w, hh, fg);
    } else if (ch >= 0x2589 && ch <= 0x258F) {  // left seven eighths .. one eighth
      g_row.fillRect(x0, 0, w * (int)(0x2590 - ch) / 8, h, fg);
    } else if (ch == 0x2590) g_row.fillRect(x0 + w / 2, 0, w - w / 2, h, fg);
    else if (ch >= 0x2591 && ch <= 0x2593) g_row.fillRect(x0, 0, w, h, blend(fg, bg, 25 * (int)(ch - 0x2590)));
    else if (ch == 0x2594) g_row.fillRect(x0, 0, w, h / 8, fg);
    else if (ch == 0x2595) g_row.fillRect(x0 + w - w / 8, 0, w / 8, h, fg);
    else return false;
    return true;
  }
  if (ch < 0x2500 || ch > 0x257F) return false;
  const BoxGlyph *g = nullptr;
  for (const auto &b : kBox) if (b.ch == ch) g = &b;
  if (!g) return false;
  const int cx = x0 + w / 2, cy = h / 2;
  // Stroke from the centre out in one direction (0 up, 1 right, 2 down, 3 left).
  auto stroke = [&](int dir, int weight) {
    if (!weight) return;
    const int t = weight == 2 ? 4 : 2;  // thickness
    const int offs[2] = {weight == 3 ? -2 : 0, weight == 3 ? 2 : 0};
    for (int k = 0; k < (weight == 3 ? 2 : 1); k++) {
      const int o = offs[k];
      switch (dir) {
        case 0: g_row.fillRect(cx - t / 2 + o, 0, weight == 3 ? 1 : t, cy + t / 2, fg); break;
        case 2: g_row.fillRect(cx - t / 2 + o, cy - t / 2, weight == 3 ? 1 : t, h - cy + t / 2, fg); break;
        case 1: g_row.fillRect(cx - t / 2, cy - t / 2 + o, x0 + w - cx + t / 2, weight == 3 ? 1 : t, fg); break;
        case 3: g_row.fillRect(x0, cy - t / 2 + o, cx - x0 + t / 2, weight == 3 ? 1 : t, fg); break;
      }
    }
  };
  for (int dir = 0; dir < 4; dir++) stroke(dir, (g->urdl >> (12 - 4 * dir)) & 0xF);
  return true;
}

struct FrameCtx {
  t5m_cursor cursor;  // filled in before the first row is drawn
  uint32_t bg;
};

void drawRow(void *ctx, int y, const t5m_cell *cells, int n) {
  const FrameCtx &f = *(const FrameCtx *)ctx;
  g_row.fillSprite(f.bg);
  // Backgrounds first: a wide glyph spills into the next cell.
  for (int x = 0; x < n; x++)
    if (cells[x].bg != f.bg) g_row.fillRect(x * g_cw, 0, g_cw, g_ch, cells[x].bg);
  const bool cursorHere = f.cursor.visible && f.cursor.y == y && g_scrolledBack == 0;
  if (cursorHere && f.cursor.x < n && f.cursor.shape == 0)
    g_row.fillRect(f.cursor.x * g_cw, 0, g_cw, g_ch, f.cursor.color);
  for (int x = 0; x < n; x++) {
    const t5m_cell &c = cells[x];
    if (c.width == 0 || c.ch <= ' ' || (c.style & 16)) continue;  // continuation, blank, invisible
    uint32_t fg = c.fg;
    if (cursorHere && x == f.cursor.x && f.cursor.shape == 0) fg = c.bg;
    if (c.style & 32) fg = (fg >> 1) & 0x7F7F7F;  // dim
    if (drawBox(c.ch, x * g_cw, fg, cursorHere && x == f.cursor.x ? f.cursor.color : c.bg)) continue;
    g_row.setTextColor(fg);
    if (c.ch < 0x10000) {
      g_row.drawChar((uint16_t)c.ch, x * g_cw, g_base);
      if (c.style & 1) g_row.drawChar((uint16_t)c.ch, x * g_cw + 1, g_base);  // bold
    }
  }
  for (int x = 0; x < n; x++) {
    const t5m_cell &c = cells[x];
    if (c.style & 2) g_row.drawFastHLine(x * g_cw, g_ch - 2, g_cw, c.fg);
    if (c.style & 128) g_row.drawFastHLine(x * g_cw, g_ch / 2, g_cw, c.fg);
  }
  if (cursorHere && f.cursor.x < n) {
    if (f.cursor.shape == 1) g_row.fillRect(f.cursor.x * g_cw, g_ch - 3, g_cw, 3, f.cursor.color);
    if (f.cursor.shape == 2) g_row.fillRect(f.cursor.x * g_cw, 0, 2, g_ch, f.cursor.color);
  }
  g_row.pushSprite(0, g_top + y * g_ch);
}

void drawBar() {
  auto &d = M5.Display;
  d.startWrite();
  d.fillRect(0, 0, kW, kBarH, kBarBg);
  const Rect r = homeButton();
  d.fillRoundRect(r.x, r.y, r.w, r.h, 10, g_theme->key);
  d.setFont(&fonts::DejaVu24);
  d.setTextDatum(textdatum_t::middle_center);
  d.setTextColor(g_theme->text);
  d.drawString("Home", r.x + r.w / 2, r.y + r.h / 2);
  const Rect su = setupButton();
  d.fillRoundRect(su.x, su.y, su.w, su.h, 10, g_setupOpen ? g_theme->keyLit : g_theme->key);
  d.drawString(g_setupOpen ? "Done" : "Setup", su.x + su.w / 2, su.y + su.h / 2);

  // Connection on the left; the program's title (OSC 0/2) on the right.
  char conn[96], line[40];
  lineName(line, sizeof line);
  if (g_setupOpen) snprintf(conn, sizeof conn, "Terminal setup");
  else if (g_set.conn == Conn::Loopback) snprintf(conn, sizeof conn, "Loopback");
  else if (g_set.conn == Conn::Ssh) sshBarText(conn, sizeof conn);
  else if (usbserial::adapter()) snprintf(conn, sizeof conn, "%s  %s", usbserial::adapter(), line);
  else if (usbserial::problem()[0]) snprintf(conn, sizeof conn, "USB-A: %s", usbserial::problem());
  else snprintf(conn, sizeof conn, "Serial: plug an adapter into USB-A");
  d.setTextDatum(textdatum_t::middle_left);
  d.drawString(conn, r.x + r.w + 24, kBarH / 2);
  char right[96];
  t5m_term_title(g_term, right, sizeof right);
  if (g_scrolledBack) snprintf(right, sizeof right, "%d lines back", g_scrolledBack);
  if (right[0] && !g_setupOpen) {
    d.setTextDatum(textdatum_t::middle_right);
    d.setTextColor(kDim);
    d.drawString(right, su.x - 36 - kBatteryW, kBarH / 2);
  }
  batteryDraw(d, su.x - 16);
  d.endWrite();
}

// Sizes the grid for the space left by the on-screen keyboard (if shown).
void layout() {
  g_kbPresent = g_kb->present();
  const int bottom = g_kbPresent ? kH : kH - kKeypadH;
  const int cols = kW / g_cw, rows = (bottom - kBarH) / g_ch;
  g_top = kBarH;
  M5.Display.fillRect(0, kBarH, kW, kH - kBarH, g_theme->background);
  if (cols != g_cols || rows != g_rows) {
    g_cols = cols;
    g_rows = rows;
    t5m_term_resize(g_term, cols, rows);
    ssh::resize(cols, rows);
  }
  if (g_kbPresent) {
    g_keypad.setRect(Rect());
  } else {
    g_keypad.setRect({0, kH - kKeypadH, kW, kKeypadH});
    g_keypadDraw.invalidate();
  }
  g_full = true;
}

// ------------------------------------------------------------------ input --

void goLive() {
  if (!g_scrolledBack) return;
  t5m_term_scroll_live(g_term);
  g_scrolledBack = 0;
  g_full = true;
  drawBar();
}

// A key from either keyboard, as SwiftTerm wants it.
void keyEvent(const KeyEvent &e) {
  if (!e.down) return;
  if (e.usage == key::CapsLock) {
    g_caps = !g_caps;
    g_keypad.setHostLeds(g_caps ? led::CapsLock : 0);
    return;
  }
  KeyText k;
  if (!KeyText::from(e, &k)) return;
  goLive();
  const bool shift = e.mods & (mod::LShift | mod::RShift);
  const uint32_t mods = (shift ? 1 : 0) | (k.alt ? 2 : 0) | (k.ctrl ? 4 : 0);
  if (k.ch) {
    char c = k.ch;
    if (g_caps && c >= 'a' && c <= 'z') c -= 32;
    else if (g_caps && c >= 'A' && c <= 'Z') c += 32;
    if (k.ctrl || k.alt) {
      // With modifiers it's a key, not text: Ctrl-C, Alt-x, ...
      char name[2] = {(char)(c >= 'A' && c <= 'Z' ? c + 32 : c), 0};
      char code[8] = "";
      if (name[0] >= 'a' && name[0] <= 'z') snprintf(code, sizeof code, "Key%c", name[0] - 32);
      t5m_term_key(g_term, name, code, mods);
    } else {
      t5m_term_text(g_term, (const uint8_t *)&c, 1);
    }
    return;
  }
  const char *name = nullptr;
  char fname[4];
  switch (k.special) {
    case key::Enter: name = "Enter"; break;
    case key::Escape: name = "Escape"; break;
    case key::Backspace: name = "Backspace"; break;
    case key::Tab: name = "Tab"; break;
    case key::Insert: name = "Insert"; break;
    case key::Delete: name = "Delete"; break;
    case key::Home: name = "Home"; break;
    case key::End: name = "End"; break;
    case key::PageUp: name = "PageUp"; break;
    case key::PageDown: name = "PageDown"; break;
    case key::Up: name = "ArrowUp"; break;
    case key::Down: name = "ArrowDown"; break;
    case key::Left: name = "ArrowLeft"; break;
    case key::Right: name = "ArrowRight"; break;
    default:
      if (k.special >= key::F1 && k.special <= key::F1 + 11) {
        snprintf(fname, sizeof fname, "F%d", k.special - key::F1 + 1);
        name = fname;
      }
  }
  if (name) t5m_term_key(g_term, name, name, mods);
}

// Drag on the grid scrolls back through earlier output.
int g_dragId = -1, g_dragY = 0;

void handleTouch() {
  TouchPoint t[kMaxTouches];
  const int n = readM5Touch(t);
  for (int i = 0; i < M5.Touch.getCount(); i++) {
    const auto &d = M5.Touch.getDetail(i);
    if (!d.wasPressed()) continue;
    if (homeButton().contains(d.x, d.y)) appGoHome();
    else if (setupButton().contains(d.x, d.y)) openSetup(!g_setupOpen);
    else if (g_setupOpen && d.y >= kBarH) setupTap(d.x, d.y);
  }
  if (g_setupOpen) return;
  if (g_keypad.rect().w > 0) g_keypad.handle(t, n);
  const int gridBottom = g_top + g_rows * g_ch;
  bool held = false;
  for (int i = 0; i < n; i++) {
    if (!t[i].down) continue;
    if (g_dragId < 0 && t[i].y >= g_top && t[i].y < gridBottom) {
      g_dragId = t[i].id;
      g_dragY = t[i].y;
    }
    if (t[i].id != g_dragId) continue;
    held = true;
    const int lines = (g_dragY - t[i].y) / g_ch;  // finger up = newer output
    if (lines) {
      g_dragY -= lines * g_ch;
      const int back = t5m_term_scroll(g_term, lines);
      if (back != g_scrolledBack) {
        g_scrolledBack = back;
        drawBar();
      }
    }
  }
  if (!held) g_dragId = -1;
}

// ---------------------------------------------------------- setup screen --

constexpr int kRowH = 64;
const char *const kRowNames[] = {"Connection", "Baud rate", "Data bits", "Parity", "Stop bits",
                                 "Flow control", "Enter sends", "Incoming line feed", "Local echo",
                                 "SSH: this Tab5's key"};
constexpr int kSetupRows = sizeof(kRowNames) / sizeof(kRowNames[0]);

void setupValue(int i, char *buf, size_t n) {
  static const char *const kParity[] = {"none", "odd", "even", "mark", "space"};
  static const char *const kEnter[] = {"CR", "CR + LF", "LF"};
  switch (i) {
    case 0:
      snprintf(buf, n, "%s", g_set.conn == Conn::Serial ? "USB serial adapter (USB-A)"
                             : g_set.conn == Conn::Ssh  ? "SSH (Wi-Fi)"
                                                        : "Loopback");
      break;
    case 1: snprintf(buf, n, "%lu", (unsigned long)kBauds[g_set.baud]); break;
    case 2: snprintf(buf, n, "%u", g_set.dataBits); break;
    case 3: snprintf(buf, n, "%s", kParity[g_set.parity % 5]); break;
    case 4: snprintf(buf, n, "%s", g_set.stopBits == 0 ? "1" : g_set.stopBits == 1 ? "1.5" : "2"); break;
    case 5: snprintf(buf, n, "%s", g_set.rtscts ? "RTS/CTS" : "none"); break;
    case 6: snprintf(buf, n, "%s", kEnter[g_set.enter % 3]); break;
    case 7: snprintf(buf, n, "%s", g_set.lfToCrlf ? "new line (CR + LF)" : "as sent"); break;
    case 8: snprintf(buf, n, "%s", g_set.echo ? "on" : "off"); break;
    case 9: snprintf(buf, n, "show"); break;
    default: buf[0] = 0;
  }
}

void drawSetup() {
  auto &d = M5.Display;
  d.startWrite();
  d.fillRect(0, kBarH, kW, kH - kBarH, g_theme->background);
  for (int i = 0; i < kSetupRows; i++) {
    const int y = kBarH + 10 + i * kRowH;
    d.fillRoundRect(20, y, kW - 40, kRowH - 8, 10, g_theme->key);
    d.setFont(&fonts::DejaVu24);
    d.setTextColor(g_theme->text);
    d.setTextDatum(textdatum_t::middle_left);
    d.drawString(kRowNames[i], 40, y + (kRowH - 8) / 2);
    char v[48];
    setupValue(i, v, sizeof v);
    d.setTextDatum(textdatum_t::middle_right);
    if (i == 1) {  // baud: - and +
      d.drawString("-", kW - 300, y + (kRowH - 8) / 2);
      d.drawString(v, kW - 160, y + (kRowH - 8) / 2);
      d.drawString("+", kW - 50, y + (kRowH - 8) / 2);
    } else {
      d.drawString(v, kW - 50, y + (kRowH - 8) / 2);
    }
  }
  d.endWrite();
}

void setupTap(int x, int y) {
  const int i = (y - kBarH - 10) / kRowH;
  if (i < 0 || i >= kSetupRows) return;
  switch (i) {
    case 0:
      g_set.conn = (Conn)(((uint8_t)g_set.conn + 1) % (uint8_t)Conn::kCount);
      if (g_set.conn == Conn::Serial) usbserial::begin();
      g_connChanged = true;
      break;
    case 1:
      if (x < kW - 230) g_set.baud = g_set.baud ? g_set.baud - 1 : 0;
      else if (g_set.baud + 1 < kBaudCount) g_set.baud++;
      break;
    case 2: g_set.dataBits = g_set.dataBits == 5 ? 8 : g_set.dataBits - 1; break;
    case 3: g_set.parity = (g_set.parity + 1) % 5; break;
    case 4: g_set.stopBits = (g_set.stopBits + 1) % 3; break;
    case 5: g_set.rtscts = !g_set.rtscts; break;
    case 6: g_set.enter = (g_set.enter + 1) % 3; break;
    case 7: g_set.lfToCrlf = !g_set.lfToCrlf; break;
    case 8: g_set.echo = !g_set.echo; break;
    case 9:
      openSetup(false);
      showKey();
      return;
  }
  if (i >= 1 && i <= 5) usbserial::setLine(serialLine());
  saveTermSettings();
  drawSetup();
}

void openSetup(bool open) {
  g_setupOpen = open;
  if (open) {
    drawSetup();
  } else {
    layout();
  }
  drawBar();
}

// ------------------------------------------------------------- connection --

// Incoming bytes to the screen, with the line-feed option.
void show(const uint8_t *data, int n) {
  if (!g_set.lfToCrlf) {
    t5m_term_feed(g_term, data, n);
    return;
  }
  int start = 0;
  for (int i = 0; i < n; i++) {
    if (data[i] != '\n') continue;
    if (i > start) t5m_term_feed(g_term, data + start, i - start);
    t5m_term_feed(g_term, (const uint8_t *)"\r\n", 2);
    start = i + 1;
  }
  if (n > start) t5m_term_feed(g_term, data + start, n - start);
}

// ------------------------------------------------------------------- SSH --

void say(const char *s) { t5m_term_feed(g_term, (const uint8_t *)s, strlen(s)); }

// Recent hosts ("user@host:port") and remembered host keys, in NVS.
constexpr int kRecent = 5;
std::string g_recent[kRecent];
std::string g_target;  // user@host:port being connected to

void loadRecent() {
  Preferences p;
  if (!p.begin("terminal", false)) return;
  for (int i = 0; i < kRecent; i++) {
    const std::string k = "r" + std::to_string(i);
    g_recent[i] = p.isKey(k.c_str()) ? p.getString(k.c_str(), "").c_str() : "";
  }
  p.end();
}

void rememberRecent(const std::string &t) {
  std::string list[kRecent];
  int n = 0;
  list[n++] = t;
  for (int i = 0; i < kRecent && n < kRecent; i++)
    if (!g_recent[i].empty() && g_recent[i] != t) list[n++] = g_recent[i];
  Preferences p;
  if (!p.begin("terminal", false)) return;
  for (int i = 0; i < kRecent; i++) {
    g_recent[i] = i < n ? list[i] : "";
    p.putString(("r" + std::to_string(i)).c_str(), g_recent[i].c_str());
  }
  p.end();
}

// NVS keys are 15 characters at most: hash host:port.
std::string hostKeyName(const std::string &hostPort) {
  uint32_t h = 2166136261u;
  for (char c : hostPort) h = (h ^ (uint8_t)c) * 16777619u;
  char k[12];
  snprintf(k, sizeof k, "k%08lx", (unsigned long)h);
  return k;
}

std::string knownHostKey(const std::string &hostPort) {
  Preferences p;
  if (!p.begin("sshhosts", false)) return "";
  const std::string k = hostKeyName(hostPort);
  std::string v = p.isKey(k.c_str()) ? p.getString(k.c_str(), "").c_str() : "";
  p.end();
  return v;
}

void saveHostKey(const std::string &hostPort, const char *fp) {
  Preferences p;
  if (!p.begin("sshhosts", false)) return;
  p.putString(hostKeyName(hostPort).c_str(), fp);
  p.end();
}

// The device key: made once, kept in NVS.
void loadDeviceKey() {
  Preferences p;
  if (!p.begin("terminal", false)) return;
  String pem = p.isKey("sshkey") ? p.getString("sshkey", "") : String();
  if (pem.isEmpty()) {
    static char buf[512];
    if (ssh::generateDeviceKey(buf, sizeof buf)) {
      p.putString("sshkey", buf);
      pem = buf;
    }
  }
  p.end();
  ssh::setDeviceKey(pem.c_str());
}

void showKey() {
  say("\r\n\x1b[1mThis Tab5's SSH key\x1b[0m - add it to ~/.ssh/authorized_keys on a server to log in\r\n"
      "without a password:\r\n\r\n");
  say(ssh::publicKey());
  say("\r\n\r\n");
  if (WiFi.status() == WL_CONNECTED) {
    char buf[96];
    snprintf(buf, sizeof buf, "Also at http://%s/ssh-key.pub, e.g.\r\n  curl http://%s/ssh-key.pub >> ~/.ssh/authorized_keys\r\n\r\n",
             WiFi.localIP().toString().c_str(), WiFi.localIP().toString().c_str());
    say(buf);
  }
}

// A line typed locally (host, yes/no, password) while no session is open.
enum class Ask : uint8_t { None, Host, Trust, Password };
Ask g_ask = Ask::None;
std::string g_lineBuf;

void askHost() {
  say("\r\n\x1b[1mSSH\x1b[0m");
  bool any = false;
  for (int i = 0; i < kRecent; i++) {
    if (g_recent[i].empty()) continue;
    char buf[96];
    snprintf(buf, sizeof buf, "%s  %d) %s", any ? "" : "  recent:", i + 1, g_recent[i].c_str());
    say(buf);
    any = true;
  }
  say("\r\nConnect to (user@host[:port]): ");
  g_ask = Ask::Host;
  g_lineBuf.clear();
}

void startSsh(const std::string &typed) {
  std::string t = typed;
  if (t.size() == 1 && t[0] >= '1' && t[0] < '1' + kRecent && !g_recent[t[0] - '1'].empty()) t = g_recent[t[0] - '1'];
  const size_t at = t.find('@');
  if (at == std::string::npos || at == 0 || at + 1 == t.size()) {
    say("\r\nType it as user@host, or user@host:port.\r\n");
    askHost();
    return;
  }
  std::string user = t.substr(0, at), host = t.substr(at + 1);
  uint16_t port = 22;
  const size_t colon = host.rfind(':');
  if (colon != std::string::npos) {
    port = (uint16_t)atoi(host.c_str() + colon + 1);
    host.resize(colon);
    if (!port) port = 22;
  }
  g_target = user + "@" + host + ":" + std::to_string(port);
  rememberRecent(port == 22 ? user + "@" + host : g_target);
  g_ask = Ask::None;
  say("\r\n");
  ssh::resize(g_cols, g_rows);
  ssh::connect(user.c_str(), host.c_str(), port, knownHostKey(host + ":" + std::to_string(port)).c_str());
}

void lineDone() {
  const Ask was = g_ask;
  g_ask = Ask::None;
  std::string line;
  line.swap(g_lineBuf);
  if (was == Ask::Host) {
    if (line.empty()) askHost();
    else startSsh(line);
  } else if (was == Ask::Trust) {
    const bool yes = line == "yes" || line == "y";
    if (yes) saveHostKey(g_target.substr(g_target.find('@') + 1), ssh::hostKey());
    say("\r\n");
    ssh::trustHostKey(yes);
  } else if (was == Ask::Password) {
    say("\r\n");
    ssh::password(line.c_str());
    line.assign(line.size(), '\0');
  }
}

// Bytes from the keyboard while answering a question.
void lineInput(const uint8_t *data, int n) {
  for (int i = 0; i < n; i++) {
    const uint8_t c = data[i];
    if (c == '\r') {
      lineDone();
    } else if (c == 0x7F || c == 0x08) {
      if (!g_lineBuf.empty()) {
        g_lineBuf.pop_back();
        if (g_ask != Ask::Password) say("\b \b");
      }
    } else if (c == 0x03) {  // Ctrl-C: give up on this question
      g_lineBuf.clear();
      if (g_ask == Ask::Trust) ssh::trustHostKey(false);
      else if (g_ask == Ask::Password) ssh::disconnect();
      else { say("^C"); askHost(); }
      if (g_ask != Ask::Host) g_ask = Ask::None;
    } else if (c >= ' ' && c < 0x7F && g_lineBuf.size() < 120) {
      g_lineBuf += (char)c;
      if (g_ask != Ask::Password) t5m_term_feed(g_term, &c, 1);
    }
  }
}

// Follows the session's state: asks the next question, says what happened.
void sshTick() {
  static uint32_t gen = 0;
  if (ssh::generation() == gen) return;
  gen = ssh::generation();
  char buf[256];
  switch (ssh::state()) {
    case ssh::State::Connecting:
      snprintf(buf, sizeof buf, "\x1b[2m%s\x1b[0m\r\n", ssh::message());
      say(buf);
      break;
    case ssh::State::HostKey:
      snprintf(buf, sizeof buf,
               "The host %s is new to this Tab5.\r\n%s key fingerprint is %s\r\n"
               "Trust it and connect (yes/no)? ",
               g_target.c_str(), ssh::hostKeyType(), ssh::hostKey());
      say(buf);
      g_ask = Ask::Trust;
      g_lineBuf.clear();
      break;
    case ssh::State::Password:
      if (ssh::message()[0]) {
        snprintf(buf, sizeof buf, "%s\r\n", ssh::message());
        say(buf);
      }
      snprintf(buf, sizeof buf, "%s's password: ", g_target.substr(0, g_target.rfind(':')).c_str());
      say(buf);
      g_ask = Ask::Password;
      g_lineBuf.clear();
      break;
    case ssh::State::Open:
      g_ask = Ask::None;
      break;
    case ssh::State::Closed:
      snprintf(buf, sizeof buf, "\r\n\x1b[1m%s\x1b[0m\r\n", ssh::message());
      say(buf);
      if (g_set.conn == Conn::Ssh) askHost();
      break;
    case ssh::State::Idle:
      break;
  }
  drawBar();
}

void sshBarText(char *buf, size_t n) {
  switch (ssh::state()) {
    case ssh::State::Open: snprintf(buf, n, "SSH  %s", g_target.c_str()); break;
    case ssh::State::Connecting:
    case ssh::State::HostKey:
    case ssh::State::Password: snprintf(buf, n, "SSH  connecting to %s", g_target.c_str()); break;
    default: snprintf(buf, n, "SSH  not connected"); break;
  }
}

// Enter ~ . disconnects, as in OpenSSH.
void sshSend(const uint8_t *data, int n) {
  static bool lineStart = true, tilde = false;
  for (int i = 0; i < n; i++) {
    const uint8_t c = data[i];
    if (tilde) {
      tilde = false;
      if (c == '.') {
        ssh::disconnect();
        return;
      }
      if (c != '~') ssh::write((const uint8_t *)"~", 1);
    } else if (lineStart && c == '~') {
      tilde = true;
      continue;
    }
    ssh::write(&c, 1);
    lineStart = c == '\r' || c == '\n';
  }
}

// What the terminal sends goes to the connection; what comes back is shown.
void pumpConnection() {
  uint8_t buf[512];
  int n;
  while ((n = t5m_term_output(g_term, buf, sizeof buf / 2)) > 0) {
    // Enter: SwiftTerm sends CR.
    uint8_t out[sizeof buf];
    int m = 0;
    for (int i = 0; i < n; i++) {
      if (buf[i] == '\r' && g_set.enter == 1) { out[m++] = '\r'; out[m++] = '\n'; }
      else if (buf[i] == '\r' && g_set.enter == 2) out[m++] = '\n';
      else out[m++] = buf[i];
    }
    if (g_set.conn == Conn::Loopback) {
      // Back to the screen, Enter as a new line and Backspace rubbing out.
      for (int i = 0; i < m; i++) {
        if (out[i] == '\r') t5m_term_feed(g_term, (const uint8_t *)"\r\n", 2);
        else if (out[i] == '\n') continue;
        else if (out[i] == 0x7F || out[i] == 0x08) t5m_term_feed(g_term, (const uint8_t *)"\b \b", 3);
        else t5m_term_feed(g_term, &out[i], 1);
      }
      continue;
    }
    if (g_set.conn == Conn::Ssh) {
      if (ssh::state() == ssh::State::Open) sshSend(out, m);
      else if (g_ask != Ask::None) lineInput(buf, n);  // raw keys: Enter is CR
      continue;
    }
    usbserial::write(out, m);
    if (g_set.echo) show(out, m);
  }
  if (g_set.conn == Conn::Ssh) {
    for (int k = 0; k < 8 && (n = ssh::read(buf, sizeof buf)) > 0; k++) t5m_term_feed(g_term, buf, n);
    sshTick();
  }
  if (g_set.conn == Conn::Serial) {
    // A screenful at a time, so a fast sender can't starve the screen and keys.
    for (int k = 0; k < 8 && (n = usbserial::read(buf, sizeof buf)) > 0; k++) show(buf, n);
  }
}

void banner() {
  static const char kText[] =
      "\x1b[1mTab5 Multitool terminal\x1b[0m - SwiftTerm by Miguel de Icaza, in Embedded Swift\r\n"
      "Setup picks the connection: a USB serial adapter on USB-A, SSH, or Loopback.\r\n\r\n"
      "\x1b[31mred \x1b[32mgreen \x1b[33myellow \x1b[34mblue \x1b[35mmagenta \x1b[36mcyan \x1b[0m"
      "\x1b[7m inverse \x1b[0m \x1b[4munderline\x1b[0m \x1b[2mdim\x1b[0m\r\n"
      "\x1b[38;2;255;128;0mtrue colour\x1b[0m  "
      "\xe2\x94\x8c\xe2\x94\x80\xe2\x94\x80\xe2\x94\x90 \xe2\x95\x94\xe2\x95\x90\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x93\xe2\x96\x92\xe2\x96\x91"
      "  \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\r\n"
      "             \xe2\x94\x94\xe2\x94\x80\xe2\x94\x80\xe2\x94\x98 \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d\r\n\r\n";
  t5m_term_feed(g_term, (const uint8_t *)kText, sizeof kText - 1);
}

}  // namespace

void terminalBegin(Tab5Keyboard &kb, const Theme &theme) {
  g_kb = &kb;
  g_theme = &theme;
  auto &d = M5.Display;
  d.setFont(kFont);
  g_cw = d.textWidth("M");
  g_ch = d.fontHeight();
  lgfx::FontMetrics m;
  kFont->getDefaultMetric(&m);
  g_base = m.baseline;
  const bool again = g_term != nullptr;  // back from the home screen
  if (!again) {
    g_row.setPsram(true);
    g_row.setColorDepth(16);
    g_row.createSprite(kW, g_ch);
    g_row.setFont(kFont);
    g_row.setTextDatum(textdatum_t::top_left);
    g_keypad.setPages(kPages, 3);
    loadTermSettings();
    g_term = t5m_term_new(kW / g_cw, (kH - kBarH) / g_ch, kScrollback);
    loadRecent();
    loadDeviceKey();
  }
  usbserial::setLine(serialLine());
  if (g_set.conn == Conn::Serial) usbserial::begin();
  g_setupOpen = false;
  d.fillScreen(theme.background);
  g_kbPresent = !g_kb->present();  // force layout() to look again
  layout();
  drawBar();
  if (!again) banner();
  // SSH: ask for a host unless a question is already on screen. A session
  // ended by leaving reports itself (and asks) through sshTick().
  if (g_set.conn == Conn::Ssh && g_ask == Ask::None && ssh::state() != ssh::State::Open &&
      ssh::state() != ssh::State::Connecting)
    askHost();
}

void terminalEnd() {
  ssh::disconnect();
  usbserial::end();
  g_dragId = -1;
}

void terminalLoop(uint32_t now) {
  (void)now;
  if (g_kb->present() != g_kbPresent && !g_setupOpen) layout();
  handleTouch();
  KeyEvent e;
  while (g_kb->next(&e)) {
    if (e.down && e.usage == Tab5Keyboard::kAppKey) appGoHome();
    keyEvent(e);
  }
  while (g_keypad.next(&e)) keyEvent(e);
  if (g_connChanged && !g_setupOpen) {
    g_connChanged = false;
    if (g_set.conn != Conn::Ssh) ssh::disconnect();
    else if (g_ask == Ask::None && ssh::state() != ssh::State::Open) askHost();
  }
  pumpConnection();

  // The bar follows the program's title and the adapter coming and going.
  static uint32_t titleGen = 0, serialGen = 0;
  char title[8];
  const uint32_t gen = t5m_term_title(g_term, title, sizeof title);
  if (gen != titleGen || usbserial::generation() != serialGen) {
    titleGen = gen;
    serialGen = usbserial::generation();
    drawBar();
  }
  if (g_setupOpen) return;
  FrameCtx f;
  M5.Display.startWrite();
  t5m_term_frame(g_term, g_full, drawRow, &f, &f.cursor, &f.bg);
  if (g_keypad.rect().w > 0) g_keypadDraw.draw(M5.Display, g_keypad, *g_theme);
  M5.Display.endWrite();
  g_full = false;
}

void terminalFeed(const uint8_t *data, int n) { if (g_term) t5m_term_feed(g_term, data, n); }
void terminalType(const char *text) { if (g_term) t5m_term_text(g_term, (const uint8_t *)text, strlen(text)); }

}  // namespace mt
