// The Units app: units.cpp has the tables and the parsing, this file draws
// them and takes input.
//
// Screen: the kinds of unit as a grid on the left; on the right the From card
// (what is typed, and its unit), the To card (the answer, and its unit), and
// below them the value in every unit of the kind (tap one to make it the To
// unit). Without the clip-on keyboard a number pad sits beside that list.
// Tapping a card's unit opens a picker of that kind's units in the list's place.
//
// The From field takes arithmetic ("3/8", "12*2.54") and mixed units of the
// kind ("5 ft 3 in", "1h 20m"). Typing a whole line with units ("100 F to C",
// "3/8 in mm") switches the kind and units to match as it is typed.
// Keys: Tab swaps, Up/Down change the kind, Esc clears.
//
// The kind, each kind's units and the typed text are kept in NVS ("units").
#include "battery.h"
#include "units_app.h"

#include <M5Unified.h>
#include <Preferences.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

#include "units.h"

using namespace t5m;
namespace u = mt::units;

namespace mt {
namespace {

constexpr int kW = 1280, kH = 720, kBarH = 64;
constexpr uint32_t kBarBg = 0x080A0C, kDim = 0x8A949E, kFaint = 0x56606A, kWhite = 0xFFFFFF;
constexpr uint32_t kPanel = 0x161B21, kPanel2 = 0x1E252D, kAccent = 0x7FB8FF, kAnswer = 0x9FE6A0, kErr = 0xFF8A7A;
constexpr uint32_t kChip = 0x232A32, kDigitKey = 0x3A424C, kFnKey = 0x2C333B;

Tab5Keyboard *g_kb = nullptr;
const Theme *g_theme = nullptr;
bool g_loaded = false;
bool g_dirty = false;
uint32_t g_changedAt = 0;

// What is chosen.
int g_cat = u::kLength;
uint8_t g_from[u::kCatCount], g_to[u::kCatCount];
std::string g_text = "1";
bool g_kbPresent = false;

// What the From field means now (worked out whenever the text changes).
struct Reading {
  bool ok = false;
  double value = 0;       // in the From unit
  bool resultOk = false;
  double result = 0;      // in the To unit
  const char *error = "";
} g_read;

// The bottom area shows the list of every unit, or a unit picker.
enum class Pick : uint8_t { None, From, To };
Pick g_pick = Pick::None;
int g_listTop = 0;  // first row shown (the list scrolls by dragging)

// ---------------------------------------------------------------- layout --

constexpr int kCatX = 16, kCatY = kBarH + 12, kCatCols = 2, kCatW = 200, kCatGap = 6;
constexpr int kCatRows = (u::kCatCount + kCatCols - 1) / kCatCols;
constexpr int kCatH = (kH - 16 - kCatY - (kCatRows - 1) * kCatGap) / kCatRows;
constexpr int kRightX = kCatX + kCatCols * kCatW + (kCatCols - 1) * kCatGap + 16;
constexpr int kRightW = kW - kRightX - 16;
constexpr int kFromY = kBarH + 12, kFromH = 112;
constexpr int kToY = kFromY + kFromH + 10, kToH = 140;
constexpr int kListY = kToY + kToH + 12, kListH = kH - 16 - kListY;
constexpr int kUnitBtnW = 250;
constexpr int kPadW = 300, kPadCols = 4, kPadRows = 4, kPadGap = 8;
constexpr int kRowH = 42;

Rect homeButton() { return {8, 6, 90, kBarH - 12}; }
Rect catRect(int i) {
  return {kCatX + (i % kCatCols) * (kCatW + kCatGap), kCatY + (i / kCatCols) * (kCatH + kCatGap), kCatW, kCatH};
}
Rect fromUnitRect() { return {kRightX + kRightW - kUnitBtnW - 12, kFromY + 12, kUnitBtnW, kFromH - 24}; }
Rect toUnitRect() { return {kRightX + kRightW - kUnitBtnW - 12, kToY + 12, kUnitBtnW, kToH - 24}; }
Rect swapRect() { return {kRightX + kRightW - kUnitBtnW - 12 - 96, kFromY + (kFromH - 50) / 2, 84, 50}; }
bool padShown() { return !g_kbPresent; }
Rect listRect() { return {kRightX, kListY, kRightW - (padShown() ? kPadW + 12 : 0), kListH}; }
Rect padRect(int i) {
  const int x0 = kRightX + kRightW - kPadW;
  const int w = (kPadW - (kPadCols - 1) * kPadGap) / kPadCols, h = (kListH - (kPadRows - 1) * kPadGap) / kPadRows;
  return {x0 + (i % kPadCols) * (w + kPadGap), kListY + (i / kPadCols) * (h + kPadGap), w, h};
}

// The number pad: what each key types (8 Backspace, 27 Esc clears, 9 Tab swaps).
struct PadKey {
  const char *label;
  char ch;
  uint32_t color;
};
const PadKey kPad[kPadCols * kPadRows] = {
    {"7", '7', kDigitKey}, {"8", '8', kDigitKey}, {"9", '9', kDigitKey}, {"<-", 8, kFnKey},
    {"4", '4', kDigitKey}, {"5", '5', kDigitKey}, {"6", '6', kDigitKey}, {"C", 27, kFnKey},
    {"1", '1', kDigitKey}, {"2", '2', kDigitKey}, {"3", '3', kDigitKey}, {"/", '/', kFnKey},
    {"0", '0', kDigitKey}, {".", '.', kDigitKey}, {"-", '-', kFnKey}, {"swap", 9, kFnKey},
};

// ------------------------------------------------------------- persistence --

void defaults() {
  for (int c = 0; c < u::kCatCount; c++) {
    g_from[c] = u::category(c)->defaultFrom;
    g_to[c] = u::category(c)->defaultTo;
  }
}

void load() {
  defaults();
  Preferences p;
  if (!p.begin("units", true)) return;
  if (p.isKey("cat")) g_cat = p.getUChar("cat", 0) % u::kCatCount;
  if (p.isKey("from") && p.getBytesLength("from") == sizeof g_from) {
    uint8_t f[u::kCatCount], t[u::kCatCount];
    p.getBytes("from", f, sizeof f);
    p.getBytes("to", t, sizeof t);
    for (int c = 0; c < u::kCatCount; c++) {
      if (f[c] < u::unitCount(c)) g_from[c] = f[c];
      if (t[c] < u::unitCount(c)) g_to[c] = t[c];
    }
  }
  if (p.isKey("text")) g_text = p.getString("text", "1").c_str();
  p.end();
}

void save() {
  Preferences p;
  if (!p.begin("units", false)) return;
  p.putUChar("cat", (uint8_t)g_cat);
  p.putBytes("from", g_from, sizeof g_from);
  p.putBytes("to", g_to, sizeof g_to);
  p.putString("text", g_text.c_str());
  p.end();
  g_dirty = false;
}

void changed() {
  g_dirty = true;
  g_changedAt = millis();
}

// ---------------------------------------------------------------- reading --

std::string fmt(double v) { return u::formatNumber(v); }

bool hasLetters(const std::string &s) {
  for (size_t i = 0; i < s.size(); i++) {
    const char c = s[i];
    // An exponent's e ("1e3") is part of a number, not a unit.
    if ((c == 'e' || c == 'E') && i > 0 && isdigit((unsigned char)s[i - 1]) && i + 1 < s.size() &&
        (isdigit((unsigned char)s[i + 1]) || s[i + 1] == '-' || s[i + 1] == '+'))
      continue;
    if (isalpha((unsigned char)c) || c == '\'' || c == '"' || (uint8_t)c >= 0x80) return true;
  }
  return false;
}

// Works out the From field. A line with units may switch the kind and units:
// returns true when it did (everything needs drawing again).
bool readText() {
  bool switched = false;
  g_read = Reading();
  if (g_text.empty()) {
    g_read.error = "";
    return false;
  }
  double v = 0;
  if (u::parseQuantity(g_text.c_str(), g_cat, g_from[g_cat], &v)) {
    g_read.ok = true;
    g_read.value = v;
  } else if (hasLetters(g_text)) {
    const u::LineResult r = u::parseLine(g_text.c_str());
    if (r.ok) {
      switched = r.cat != g_cat || r.from != g_from[r.cat] || (r.hasTarget && r.to != g_to[r.cat]);
      g_cat = r.cat;
      g_from[r.cat] = (uint8_t)r.from;
      if (r.hasTarget) g_to[r.cat] = (uint8_t)r.to;
      g_read.ok = true;
      g_read.value = r.value;
    } else {
      g_read.error = r.error && *r.error ? r.error : "not understood";
    }
  } else {
    g_read.error = "not a number";
  }
  if (g_read.ok) {
    bool ok = false;
    g_read.result = u::convert(g_read.value, g_cat, g_from[g_cat], g_to[g_cat], &ok);
    g_read.resultOk = ok;
    if (!ok) g_read.error = "no answer (out of range)";
  }
  return switched;
}

// Another way to read the answer, under it: feet and inches, a tape-measure
// fraction, or h:mm:ss.
std::string extraLine() {
  if (!g_read.resultOk) return "";
  if (g_cat == u::kLength) {
    static const int in = u::findUnit("in", u::kLength).unit;
    bool ok = false;
    const double inches = u::convert(g_read.value, g_cat, g_from[g_cat], in, &ok);
    if (!ok || inches == 0 || fabs(inches) > 1e6) return "";
    if (fabs(inches) < 12) return "= " + u::formatFraction(inches, 64, true) + " in";
    return "= " + u::formatFeetInches(inches, 16);
  }
  if (g_cat == u::kTime) {
    static const int s = u::findUnit("s", u::kTime).unit;
    bool ok = false;
    const double secs = u::convert(g_read.value, g_cat, g_from[g_cat], s, &ok);
    if (!ok || fabs(secs) < 60 || fabs(secs) > 1e9) return "";
    return "= " + u::formatDuration(secs);
  }
  return "";
}

// --------------------------------------------------------------- drawing --

// Cards and list rows are drawn into sprites, so text that changes never
// flickers.
LGFX_Sprite g_card(&M5.Display);
LGFX_Sprite g_row(&M5.Display);

void ensureSprites() {
  if (g_card.width()) return;
  g_card.setPsram(true);
  g_row.setPsram(true);
  g_card.createSprite(kRightW, kToH);
  g_row.createSprite(kRightW, kRowH);
}

void pushClipped(LGFX_Sprite &s, int x, int y, int w, int h) {
  M5.Display.setClipRect(x, y, w, h);
  s.pushSprite(x, y);
  M5.Display.clearClipRect();
}

void unitButton(LGFX_Sprite &s, const Rect &r, int y0, const u::Unit *un, bool open) {
  const int x = r.x - kRightX, y = r.y - y0;
  s.fillRoundRect(x, y, r.w, r.h, 10, open ? g_theme->keyLit : g_theme->key);
  s.setTextDatum(textdatum_t::middle_center);
  s.setTextColor(kWhite);
  s.setFont(&fonts::DejaVu24);
  s.drawString(un->symbol, x + r.w / 2, y + r.h / 2 - 13);
  s.setFont(&fonts::DejaVu12);
  s.setTextColor(open ? kWhite : kDim);
  s.drawString(un->name, x + r.w / 2, y + r.h / 2 + 18);
}

// Shortens s from the left with ".." until it fits: the end of what is typed matters most.
std::string fitLeft(LGFX_Sprite &s, std::string t, int w) {
  if (s.textWidth(t.c_str()) <= w) return t;
  while (t.size() > 1 && s.textWidth((".." + t).c_str()) > w) t.erase(0, 1);
  return ".." + t;
}

void drawFrom() {
  ensureSprites();
  auto &s = g_card;
  s.fillSprite(g_theme->background);
  s.fillRoundRect(0, 0, kRightW, kFromH, 14, kPanel);
  s.setFont(&fonts::DejaVu12);
  s.setTextDatum(textdatum_t::top_left);
  s.setTextColor(kDim);
  s.drawString("FROM", 18, 10);
  const int textW = swapRect().x - kRightX - 30;
  s.setFont(&fonts::DejaVu40);
  s.setTextDatum(textdatum_t::middle_left);
  s.setTextColor(kWhite);
  const std::string shown = fitLeft(s, g_text + "_", textW);
  s.drawString(shown.c_str(), 18, kFromH / 2 + 6);
  if (!g_read.ok && *g_read.error) {
    s.setFont(&fonts::DejaVu12);
    s.setTextDatum(textdatum_t::top_left);
    s.setTextColor(kErr);
    s.drawString(g_read.error, 80, 10);
  }
  const Rect sw = swapRect();
  s.fillRoundRect(sw.x - kRightX, sw.y - kFromY, sw.w, sw.h, 10, g_theme->key);
  s.setFont(&fonts::DejaVu18);
  s.setTextDatum(textdatum_t::middle_center);
  s.setTextColor(g_theme->text);
  s.drawString("swap", sw.x - kRightX + sw.w / 2, sw.y - kFromY + sw.h / 2);
  unitButton(s, fromUnitRect(), kFromY, u::unit(g_cat, g_from[g_cat]), g_pick == Pick::From);
  // The sprite is the taller To card's: only the From card's height goes out.
  pushClipped(s, kRightX, kFromY, kRightW, kFromH);
}

void drawTo() {
  ensureSprites();
  auto &s = g_card;
  s.fillRoundRect(0, 0, kRightW, kToH, 14, kPanel2);
  s.setFont(&fonts::DejaVu12);
  s.setTextDatum(textdatum_t::top_left);
  s.setTextColor(kDim);
  s.drawString("TO", 18, 10);
  const int textW = toUnitRect().x - kRightX - 36;
  s.setTextDatum(textdatum_t::middle_left);
  if (g_read.resultOk) {
    const std::string a = fmt(g_read.result);
    s.setFont(s.textWidth(a.c_str(), &fonts::DejaVu56) <= textW ? &fonts::DejaVu56 : &fonts::DejaVu40);
    s.setTextColor(kAnswer);
    s.drawString(a.c_str(), 18, kToH / 2 - 4);
    const std::string extra = extraLine();
    if (!extra.empty()) {
      s.setFont(&fonts::DejaVu18);
      s.setTextColor(kDim);
      s.drawString(extra.c_str(), 18, kToH - 24);
    }
  } else {
    s.setFont(&fonts::DejaVu40);
    s.setTextColor(kFaint);
    s.drawString("-", 18, kToH / 2 - 4);
  }
  const char *note = u::category(g_cat)->note;
  if (note) {
    s.setFont(&fonts::DejaVu12);
    s.setTextDatum(textdatum_t::top_right);
    s.setTextColor(kFaint);
    s.drawString(note, toUnitRect().x - kRightX - 16, 10);
  }
  unitButton(s, toUnitRect(), kToY, u::unit(g_cat, g_to[g_cat]), g_pick == Pick::To);
  s.pushSprite(kRightX, kToY);
}

void drawCat(int i) {
  auto &d = M5.Display;
  const Rect r = catRect(i);
  const bool on = i == g_cat;
  d.fillRoundRect(r.x, r.y, r.w, r.h, 8, on ? g_theme->keyLit : kChip);
  d.setFont(&fonts::DejaVu18);
  d.setTextDatum(textdatum_t::middle_center);
  d.setTextColor(on ? kWhite : g_theme->text);
  d.drawString(u::category(i)->name, r.x + r.w / 2, r.y + r.h / 2);
}

void drawCats() {
  M5.Display.startWrite();
  for (int i = 0; i < u::kCatCount; i++) drawCat(i);
  M5.Display.endWrite();
}

int listRows() { return kListH / kRowH; }

// The list: every unit with the value in it (the To unit lit), or the picker.
// Common units come first, then the rest, each in table order.
int g_order[64];
int g_orderCount = 0;

void buildOrder() {
  g_orderCount = 0;
  const int n = u::unitCount(g_cat);
  for (int pass = 0; pass < 2; pass++)
    for (int i = 0; i < n && g_orderCount < 64; i++) {
      const bool common = u::unit(g_cat, i)->flags & u::kCommon;
      if (common == (pass == 0)) g_order[g_orderCount++] = i;
    }
}

void drawList() {
  ensureSprites();
  const Rect lr = listRect();
  auto &s = g_row;
  const int rows = listRows();
  if (g_listTop > g_orderCount - rows) g_listTop = g_orderCount - rows;
  if (g_listTop < 0) g_listTop = 0;
  const int chosen = g_pick == Pick::From ? g_from[g_cat] : g_to[g_cat];
  for (int r = 0; r < rows; r++) {
    const int k = g_listTop + r;
    s.fillSprite(g_theme->background);
    if (k < g_orderCount) {
      const int i = g_order[k];
      const u::Unit *un = u::unit(g_cat, i);
      const bool lit = i == chosen;
      s.fillRoundRect(0, 1, lr.w, kRowH - 2, 8, lit ? g_theme->keyLit : (r & 1 ? kPanel : kPanel2));
      s.setFont(&fonts::DejaVu18);
      s.setTextColor(lit ? kWhite : g_theme->text);
      if (g_pick == Pick::None) {
        bool ok = false;
        const double v = g_read.ok ? u::convert(g_read.value, g_cat, g_from[g_cat], i, &ok) : 0;
        s.setTextDatum(textdatum_t::middle_right);
        s.drawString(ok ? fmt(v).c_str() : "-", lr.w * 55 / 100, kRowH / 2);
        s.setTextDatum(textdatum_t::middle_left);
        s.drawString(un->symbol, lr.w * 55 / 100 + 14, kRowH / 2);
        s.setTextColor(lit ? kWhite : kDim);
        s.setFont(&fonts::DejaVu12);
        s.drawString(un->name, lr.w * 55 / 100 + 14 + s.textWidth(un->symbol, &fonts::DejaVu18) + 14, kRowH / 2 + 1);
      } else {
        s.setTextDatum(textdatum_t::middle_left);
        s.drawString(un->symbol, 18, kRowH / 2);
        s.setTextColor(lit ? kWhite : kDim);
        s.drawString(un->name, 160, kRowH / 2);
        if (un->flags & u::kApprox) {
          s.setTextDatum(textdatum_t::middle_right);
          s.setTextColor(kFaint);
          s.drawString("approx.", lr.w - 16, kRowH / 2);
        }
      }
    }
    // Only the list's width: the pad may sit to the right.
    pushClipped(s, lr.x, lr.y + r * kRowH, lr.w, kRowH);
  }
  // Scroll hint: more rows than fit.
  if (g_orderCount > rows) {
    const int h = kListH * rows / g_orderCount, y = kListY + (kListH - h) * g_listTop / (g_orderCount - rows);
    M5.Display.fillRect(lr.x + lr.w - 4, kListY, 4, kListH, g_theme->background);
    M5.Display.fillRoundRect(lr.x + lr.w - 4, y, 4, h, 2, kFaint);
  }
}

void drawPadKey(int i, bool held) {
  auto &d = M5.Display;
  const Rect r = padRect(i);
  uint32_t c = kPad[i].color;
  if (held) c = g_theme->keyLit;
  d.fillRoundRect(r.x, r.y, r.w, r.h, 10, c);
  d.setFont(&fonts::DejaVu24);
  d.setTextDatum(textdatum_t::middle_center);
  d.setTextColor(kWhite);
  d.drawString(kPad[i].label, r.x + r.w / 2, r.y + r.h / 2);
}

void drawPad() {
  if (!padShown()) return;
  M5.Display.startWrite();
  for (int i = 0; i < kPadCols * kPadRows; i++) drawPadKey(i, false);
  M5.Display.endWrite();
}

void drawBar() {
  auto &d = M5.Display;
  d.startWrite();
  d.fillRect(0, 0, kW, kBarH, kBarBg);
  const Rect h = homeButton();
  d.fillRoundRect(h.x, h.y, h.w, h.h, 10, g_theme->key);
  d.setFont(&fonts::DejaVu24);
  d.setTextDatum(textdatum_t::middle_center);
  d.setTextColor(g_theme->text);
  d.drawString("Home", h.x + h.w / 2, h.y + h.h / 2);
  d.setTextDatum(textdatum_t::middle_left);
  d.drawString("Units", h.x + h.w + 24, kBarH / 2);
  d.setTextDatum(textdatum_t::middle_right);
  d.setTextColor(kFaint);
  d.setFont(&fonts::DejaVu18);
  d.drawString(g_kbPresent ? "type \"100 F to C\" or \"5 ft 3 in mm\"   Tab swaps   Up/Down kind" :
                             "tap a unit to change it", kW - 40 - kBatteryW, kBarH / 2);
  batteryDraw(d, kW - 16);
  d.endWrite();
}

void drawAll() {
  auto &d = M5.Display;
  d.startWrite();
  d.fillScreen(g_theme->background);
  d.endWrite();
  buildOrder();
  drawBar();
  drawCats();
  drawFrom();
  drawTo();
  drawList();
  drawPad();
}

// After the text, a unit or the kind changed.
void refresh(bool kindMayChange) {
  const int cat = g_cat;
  const bool switched = readText();
  if (switched || g_cat != cat) {
    if (g_cat != cat) {
      g_pick = Pick::None;
      g_listTop = 0;
      buildOrder();
      M5.Display.startWrite();
      drawCat(cat);
      drawCat(g_cat);
      M5.Display.endWrite();
    }
  }
  (void)kindMayChange;
  drawFrom();
  drawTo();
  drawList();
  changed();
}

// ----------------------------------------------------------------- input --

void setCat(int c) {
  c = (c % u::kCatCount + u::kCatCount) % u::kCatCount;
  if (c == g_cat) return;
  const int was = g_cat;
  g_cat = c;
  g_pick = Pick::None;
  g_listTop = 0;
  buildOrder();
  M5.Display.startWrite();
  drawCat(was);
  drawCat(g_cat);
  M5.Display.endWrite();
  refresh(false);
}

void swapUnits() {
  // The answer becomes what is typed, so swapping twice gets back to the start.
  if (g_read.resultOk && !hasLetters(g_text)) g_text = fmt(g_read.result);
  std::swap(g_from[g_cat], g_to[g_cat]);
  if (g_text.find(',') != std::string::npos) g_text.erase(std::remove(g_text.begin(), g_text.end(), ','), g_text.end());
  refresh(false);
}

void typeChar(char c) {
  switch (c) {
    case 8:
      if (!g_text.empty()) {
        // A UTF-8 character is several bytes: remove them all.
        do g_text.pop_back();
        while (!g_text.empty() && ((uint8_t)g_text.back() & 0xC0) == 0x80);
      }
      break;
    case 27: g_text.clear(); break;
    case 9: swapUnits(); return;
    default:
      if ((uint8_t)c < 0x20 || g_text.size() > 60) return;
      g_text += c;
  }
  refresh(true);
}

void typed(const KeyText &t) {
  if (t.special) {
    switch (t.special) {
      case key::Backspace: typeChar(8); break;
      case key::Escape:
        if (g_pick != Pick::None) {
          g_pick = Pick::None;
          drawFrom();
          drawTo();
          drawList();
        } else {
          typeChar(27);
        }
        break;
      case key::Tab: swapUnits(); break;
      case key::Up: setCat(g_cat - 1); break;
      case key::Down: setCat(g_cat + 1); break;
      case key::Enter:
        // A line with a target ("100 F to C") is done: keep just the number.
        if (g_read.ok && hasLetters(g_text)) {
          g_text = fmt(g_read.value);
          g_text.erase(std::remove(g_text.begin(), g_text.end(), ','), g_text.end());
          refresh(false);
        }
        break;
    }
    return;
  }
  if (t.ctrl || t.alt) return;
  typeChar(t.ch);
}

void openPicker(Pick p) {
  g_pick = g_pick == p ? Pick::None : p;
  g_listTop = 0;
  if (g_pick != Pick::None) {
    // Start the picker where the chosen unit is.
    const int chosen = p == Pick::From ? g_from[g_cat] : g_to[g_cat];
    for (int k = 0; k < g_orderCount; k++)
      if (g_order[k] == chosen && k >= listRows()) g_listTop = k - listRows() / 2;
  }
  drawFrom();
  drawTo();
  drawList();
}

void listTap(int y) {
  const int k = g_listTop + (y - kListY) / kRowH;
  if (k < 0 || k >= g_orderCount) return;
  const int i = g_order[k];
  if (g_pick == Pick::From) g_from[g_cat] = (uint8_t)i;
  else g_to[g_cat] = (uint8_t)i;
  g_pick = Pick::None;
  refresh(false);
}

int g_padHeld = -1;
int g_dragId = -1, g_dragY = 0, g_dragStartTop = 0;
bool g_dragged = false;

void tapAt(int x, int y, bool *startedDrag = nullptr) {
  if (homeButton().contains(x, y)) {
    appGoHome();
    return;
  }
  for (int i = 0; i < u::kCatCount; i++)
    if (catRect(i).contains(x, y)) return setCat(i);
  if (fromUnitRect().contains(x, y)) return openPicker(Pick::From);
  if (toUnitRect().contains(x, y)) return openPicker(Pick::To);
  if (swapRect().contains(x, y)) return swapUnits();
  if (padShown())
    for (int i = 0; i < kPadCols * kPadRows; i++)
      if (padRect(i).contains(x, y)) {
        g_padHeld = i;
        drawPadKey(i, true);
        typeChar(kPad[i].ch);
        return;
      }
  if (listRect().contains(x, y) && startedDrag) *startedDrag = true;
}

void handleTouch() {
  for (int i = 0; i < M5.Touch.getCount(); i++) {
    const auto &t = M5.Touch.getDetail(i);
    if (t.wasPressed()) {
      bool list = false;
      tapAt(t.x, t.y, &list);
      if (list) {
        // The list scrolls by dragging; a touch that doesn't move is a tap.
        g_dragId = t.id;
        g_dragY = t.y;
        g_dragStartTop = g_listTop;
        g_dragged = false;
      }
    } else if (t.id == g_dragId && t.isPressed()) {
      const int rows = (g_dragY - t.y) / kRowH;
      if (abs(g_dragY - t.y) > 16) g_dragged = true;
      if (g_dragged && g_dragStartTop + rows != g_listTop) {
        g_listTop = g_dragStartTop + rows;
        drawList();
      }
    } else if (t.wasReleased()) {
      if (t.id == g_dragId) {
        if (!g_dragged && g_pick == Pick::None) {
          // Tap a row: that unit becomes the To unit.
          const int k = g_listTop + (g_dragY - kListY) / kRowH;
          if (k >= 0 && k < g_orderCount) {
            g_to[g_cat] = (uint8_t)g_order[k];
            refresh(false);
          }
        } else if (!g_dragged) {
          listTap(g_dragY);
        }
        g_dragId = -1;
      }
    }
  }
  if (g_padHeld >= 0 && M5.Touch.getCount() == 0) {
    drawPadKey(g_padHeld, false);
    g_padHeld = -1;
  }
}

}  // namespace

void unitsBegin(Tab5Keyboard &kb, const Theme &theme) {
  g_kb = &kb;
  g_theme = &theme;
  if (!g_loaded) {
    g_loaded = true;
    load();
  }
  g_kbPresent = g_kb->present();
  g_pick = Pick::None;
  readText();
  drawAll();
}

void unitsEnd() { save(); }

void unitsLoop(uint32_t now) {
  if (g_kb->present() != g_kbPresent) {
    g_kbPresent = g_kb->present();
    drawAll();  // the number pad comes or goes
  }
  handleTouch();
  KeyEvent e;
  KeyText t;
  while (g_kb->next(&e))
    if (KeyText::from(e, &t)) typed(t);
  if (g_dirty && now - g_changedAt > 3000) save();
}

void unitsType(char c) {
  KeyText t;
  if (c == '\n' || c == '\r') t.special = key::Enter;
  else if (c == 27) t.special = key::Escape;
  else if (c == 8 || c == 127) t.special = key::Backspace;
  else if (c == '\t') t.special = key::Tab;
  else t.ch = c;
  typed(t);
}

void unitsTap(int x, int y) {
  bool list = false;
  tapAt(x, y, &list);
  if (list) {
    const int k = g_listTop + (y - kListY) / kRowH;
    if (g_pick != Pick::None) {
      listTap(y);
    } else if (k >= 0 && k < g_orderCount) {
      g_to[g_cat] = (uint8_t)g_order[k];
      refresh(false);
    }
  }
  if (g_padHeld >= 0) {
    drawPadKey(g_padHeld, false);
    g_padHeld = -1;
  }
}

}  // namespace mt
