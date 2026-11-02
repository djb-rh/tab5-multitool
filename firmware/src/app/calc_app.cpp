// The Calculator app: rpn.cpp does the arithmetic, this file draws it and
// takes keys.
//
// Screen: a bar (Home, title, modes) over the display on the left (the stack,
// LASTx, registers or the big stack, and a message line) and the keypad on the
// right: function pages (SCI, MATH, CONV, STAT, BASE, MEM, MODE) above the
// number pad. Keys act when pressed and light while held.
//
// The clip-on keyboard: digits . Enter Backspace + - * / ^ as printed, and
//   e EEX   n CHS   x x<>y   r R-down   R R-up   u (or Ctrl+Z) undo   w LASTx
//   s c t  sin cos tan (S C T the inverses)   q sqrt   i 1/x   l ln   L log
//   p pi   ! n!   %   Esc CLx   Left/Right change the function page
// In HEX, a-f are digits instead.
//
// The stack, registers and modes are kept in NVS ("calc"), saved a few seconds
// after they change and when leaving.
#include "calc_app.h"

#include <M5Unified.h>
#include <Preferences.h>

#include <string>

#include "rpn.h"

using namespace t5m;
using mt::rpn::Op;

namespace mt {
namespace {

constexpr int kW = 1280, kH = 720, kBarH = 64;
constexpr uint32_t kBarBg = 0x080A0C, kDim = 0x8A949E, kFaint = 0x56606A, kWhite = 0xFFFFFF;
constexpr uint32_t kErr = 0xFF6B5E, kAccent = 0x7FB8FF, kPrompt = 0xFFC060;
constexpr uint32_t kDigitKey = 0x3A424C, kOpKey = 0x8C5A1E, kEnterKey = 0x2A62B0, kFnKey = 0x2C333B;
constexpr uint32_t kPanel = 0x161B21;
// Labels with symbols (× ÷ √ π Σ Δ Γ ↓ ↑ →) need the Japanese font; plain ones look better in DejaVu.
const lgfx::IFont *const kSymbolFont = &fonts::lgfxJapanGothic_28;

Tab5Keyboard *g_kb = nullptr;
const Theme *g_theme = nullptr;
rpn::Calc g_calc;
bool g_loaded = false;
bool g_dirty = false;
uint32_t g_changedAt = 0;

// ------------------------------------------------------------------ keys --

enum class Act : uint8_t {
  None, Digit, Point, Eex, Chs, Back, Op, Page, Sto, Rcl, Angle, Base, Fmt, DigitsDown, DigitsUp, BigStack,
  Group, ClearAll,
};

struct Key {
  const char *label;
  Act act;
  int arg;  // the digit, the Op, the page, the arithmetic for STO/RCL, the mode
  uint32_t color;
};

#define OP(label, op) {label, Act::Op, (int)Op::op, kFnKey}
constexpr Key kBlank = {"", Act::None, 0, 0};

// Function pages: 3 rows of 6.
constexpr int kFnCols = 6, kFnRows = 3;
struct Page {
  const char *name;
  Key keys[kFnRows * kFnCols];
};
const Page kPages[] = {
    {"SCI",
     {OP("SIN", Sin), OP("COS", Cos), OP("TAN", Tan), OP("ASIN", Asin), OP("ACOS", Acos), OP("ATAN", Atan),
      OP("√x", Sqrt), OP("x^2", Sq), OP("y^x", Pow), OP("x√y", Root), OP("1/x", Inv), OP("n!", Fact),
      OP("%", Pct), OP("Δ%", DeltaPct), OP("ABS", Abs), OP("RND", Round), OP("π", Pi), OP("e", E)}},
    {"MATH",
     {OP("LN", Ln), OP("e^x", Exp), OP("LOG", Log), OP("10^x", Alog), OP("LOG2", Log2), OP("2^x", Exp2),
      OP("SINH", Sinh), OP("COSH", Cosh), OP("TANH", Tanh), OP("ASINH", Asinh), OP("ACOSH", Acosh),
      OP("ATANH", Atanh), OP("nCr", Comb), OP("nPr", Perm), OP("Γ(x)", Gamma), OP("MOD", Mod), OP("IP", IntPart),
      OP("FP", FracPart)}},
    {"CONV",
     {OP("→POL", ToPolar), OP("→REC", ToRect), OP("HYPOT", Hypot), OP("→DEG", ToDeg), OP("→RAD", ToRad),
      OP("RAND", Rand), OP("→H.MS", ToHms), OP("H.MS→", FromHms), OP("INT÷", IntDiv), kBlank, kBlank, kBlank,
      kBlank, kBlank, kBlank, kBlank, kBlank, kBlank}},
    {"STAT",
     {OP("Σ+", SigmaPlus), OP("Σ-", SigmaMinus), OP("n", StatN), OP("CLΣ", ClearSigma), kBlank, kBlank,
      OP("mean x", MeanX), OP("mean y", MeanY), OP("sx", StdDevX), OP("sy", StdDevY), kBlank, kBlank,
      OP("slope", Slope), OP("icept", Intercept), OP("r", Corr), OP("y est", Yhat), kBlank, kBlank}},
    {"BASE",
     {{"A", Act::Digit, 10, kDigitKey}, {"B", Act::Digit, 11, kDigitKey}, {"C", Act::Digit, 12, kDigitKey},
      {"D", Act::Digit, 13, kDigitKey}, {"E", Act::Digit, 14, kDigitKey}, {"F", Act::Digit, 15, kDigitKey},
      {"HEX", Act::Base, (int)rpn::Base::Hex, kFnKey}, {"DEC", Act::Base, (int)rpn::Base::Dec, kFnKey},
      {"OCT", Act::Base, (int)rpn::Base::Oct, kFnKey}, {"BIN", Act::Base, (int)rpn::Base::Bin, kFnKey},
      {"REAL", Act::Base, (int)rpn::Base::Real, kFnKey}, kBlank,
      OP("AND", And), OP("OR", Or), OP("XOR", Xor), OP("NOT", Not), OP("SL", Shl), OP("SR", Shr)}},
    {"MEM",
     {{"STO", Act::Sto, (int)Op::None, kFnKey}, {"STO+", Act::Sto, (int)Op::Add, kFnKey},
      {"STO-", Act::Sto, (int)Op::Sub, kFnKey}, {"STO×", Act::Sto, (int)Op::Mul, kFnKey},
      {"STO÷", Act::Sto, (int)Op::Div, kFnKey}, OP("CLREG", ClearRegs),
      {"RCL", Act::Rcl, (int)Op::None, kFnKey}, {"RCL+", Act::Rcl, (int)Op::Add, kFnKey},
      {"RCL-", Act::Rcl, (int)Op::Sub, kFnKey}, {"RCL×", Act::Rcl, (int)Op::Mul, kFnKey},
      {"RCL÷", Act::Rcl, (int)Op::Div, kFnKey}, OP("LASTx", LastX),
      OP("R↑", RollUp), OP("DROP", Drop), OP("CLST", ClearStack), kBlank, kBlank, kBlank}},
    {"MODE",
     {{"DEG", Act::Angle, (int)rpn::Angle::Deg, kFnKey}, {"RAD", Act::Angle, (int)rpn::Angle::Rad, kFnKey},
      {"GRAD", Act::Angle, (int)rpn::Angle::Grad, kFnKey}, kBlank, {"BIG STK", Act::BigStack, 0, kFnKey},
      {"1,000", Act::Group, 0, kFnKey},
      {"ALL", Act::Fmt, (int)rpn::Fmt::All, kFnKey}, {"FIX", Act::Fmt, (int)rpn::Fmt::Fix, kFnKey},
      {"SCI", Act::Fmt, (int)rpn::Fmt::Sci, kFnKey}, {"ENG", Act::Fmt, (int)rpn::Fmt::Eng, kFnKey},
      {"DIGITS-", Act::DigitsDown, 0, kFnKey}, {"DIGITS+", Act::DigitsUp, 0, kFnKey},
      kBlank, kBlank, kBlank, kBlank, kBlank, {"RESET", Act::ClearAll, 0, 0x6A2A2A}}},
};
constexpr int kPageCount = sizeof(kPages) / sizeof(kPages[0]);

// The number pad: 5 rows of 5. ENTER is two keys wide (the second is blank).
constexpr int kPadCols = 5, kPadRows = 5;
const Key kPad[kPadRows * kPadCols] = {
    {"ENTER", Act::Op, (int)Op::Enter, kEnterKey}, kBlank, {"x<>y", Act::Op, (int)Op::Swap, kFnKey},
    {"R↓", Act::Op, (int)Op::RollDown, kFnKey}, {"←", Act::Back, 0, kFnKey},
    {"7", Act::Digit, 7, kDigitKey}, {"8", Act::Digit, 8, kDigitKey}, {"9", Act::Digit, 9, kDigitKey},
    {"÷", Act::Op, (int)Op::Div, kOpKey}, {"UNDO", Act::Op, (int)Op::Undo, kFnKey},
    {"4", Act::Digit, 4, kDigitKey}, {"5", Act::Digit, 5, kDigitKey}, {"6", Act::Digit, 6, kDigitKey},
    {"×", Act::Op, (int)Op::Mul, kOpKey}, {"EEX", Act::Eex, 0, kFnKey},
    {"1", Act::Digit, 1, kDigitKey}, {"2", Act::Digit, 2, kDigitKey}, {"3", Act::Digit, 3, kDigitKey},
    {"-", Act::Op, (int)Op::Sub, kOpKey}, {"CHS", Act::Chs, 0, kFnKey},
    {"0", Act::Digit, 0, kDigitKey}, {".", Act::Point, 0, kDigitKey}, {"CLx", Act::Op, (int)Op::Clx, kFnKey},
    {"+", Act::Op, (int)Op::Add, kOpKey}, {"LASTx", Act::Op, (int)Op::LastX, kFnKey},
};

// Layout: the display on the left, keys on the right.
constexpr int kDispX = 16, kDispY = kBarH + 12, kDispW = 584, kDispH = kH - kDispY - 16;
constexpr int kKeysX = 616, kKeysW = kW - kKeysX - 16, kGap = 8;
constexpr int kTabY = kBarH + 12, kTabH = 50;
constexpr int kFnY = kTabY + kTabH + 10, kFnH = 64;
constexpr int kPadY = kFnY + kFnRows * (kFnH + kGap) + 8, kPadH = (kH - 16 - kPadY - (kPadRows - 1) * kGap) / kPadRows;

Rect tabRect(int i) {
  const int w = (kKeysW - (kPageCount - 1) * 6) / kPageCount;
  return {kKeysX + i * (w + 6), kTabY, w, kTabH};
}
Rect fnRect(int i) {
  const int w = (kKeysW - (kFnCols - 1) * kGap) / kFnCols;
  return {kKeysX + (i % kFnCols) * (w + kGap), kFnY + (i / kFnCols) * (kFnH + kGap), w, kFnH};
}
Rect padRect(int i) {
  const int w = (kKeysW - (kPadCols - 1) * kGap) / kPadCols;
  Rect r{kKeysX + (i % kPadCols) * (w + kGap), kPadY + (i / kPadCols) * (kPadH + kGap), w, kPadH};
  if (i == 0) r.w = 2 * w + kGap;  // ENTER
  return r;
}
Rect homeButton() { return {8, 6, 90, kBarH - 12}; }

int g_page = 0;
// What is held down: -1 none, 0..17 a function key, 100+ a pad key, 200+ a tab.
int g_pressed = -1;
int g_pressId = -1;  // the touch id holding it
// STO or RCL waiting for its register digit, with its arithmetic.
Act g_pending = Act::None;
Op g_pendingArith = Op::None;

// ------------------------------------------------------------- persistence --

void load() {
  Preferences p;
  if (!p.begin("calc", true)) return;
  if (p.isKey("state")) {
    static rpn::State s;
    const size_t n = p.getBytes("state", &s, sizeof s);
    g_calc.load(&s, n);
  }
  if (p.isKey("page")) g_page = p.getUChar("page", 0) % kPageCount;
  p.end();
}

void save() {
  Preferences p;
  if (!p.begin("calc", false)) return;
  p.putBytes("state", &g_calc.state(), sizeof(rpn::State));
  p.putUChar("page", (uint8_t)g_page);
  p.end();
  g_dirty = false;
}

void changed() {
  g_dirty = true;
  g_changedAt = millis();
}

// --------------------------------------------------------------- drawing --

bool modeLit(const Key &k) {
  switch (k.act) {
    case Act::Angle: return (int)g_calc.angle() == k.arg;
    case Act::Base: return (int)g_calc.base() == k.arg;
    case Act::Fmt: return (int)g_calc.format() == k.arg;
    case Act::BigStack: return g_calc.bigStack();
    case Act::Group: return g_calc.grouping();
    case Act::Sto: return g_pending == Act::Sto && (int)g_pendingArith == k.arg;
    case Act::Rcl: return g_pending == Act::Rcl && (int)g_pendingArith == k.arg;
    default: return false;
  }
}

// Keys that do nothing in the current mode are drawn faint.
bool usable(const Key &k) {
  if (k.act == Act::None) return false;
  const bool base = g_calc.baseMode();
  if (k.act == Act::Digit) {
    switch (g_calc.base()) {
      case rpn::Base::Bin: return k.arg < 2;
      case rpn::Base::Oct: return k.arg < 8;
      case rpn::Base::Hex: return true;
      default: return k.arg < 10;
    }
  }
  if (k.act == Act::Point || k.act == Act::Eex) return !base;
  if (k.act == Act::Op) {
    const Op op = (Op)k.arg;
    const bool baseOnly = op == Op::And || op == Op::Or || op == Op::Xor || op == Op::Not || op == Op::Shl ||
                          op == Op::Shr;
    if (baseOnly) return base;
    if (base) {
      switch (op) {
        case Op::Enter: case Op::Swap: case Op::RollDown: case Op::RollUp: case Op::Clx: case Op::ClearStack:
        case Op::LastX: case Op::Drop: case Op::Undo: case Op::Chs: case Op::Add: case Op::Sub: case Op::Mul:
        case Op::Div: case Op::Mod: case Op::IntDiv: case Op::Abs: case Op::ClearRegs:
          return true;
        default: return false;
      }
    }
  }
  return true;
}

uint32_t lighten(uint32_t c, int pct) {
  auto ch = [&](int s) {
    const int v = (c >> s) & 0xFF;
    return (uint32_t)(v + (255 - v) * pct / 100) << s;
  };
  return ch(16) | ch(8) | ch(0);
}

void drawKey(const Rect &r, const Key &k, bool held) {
  auto &d = M5.Display;
  if (k.act == Act::None) {
    d.fillRect(r.x, r.y, r.w, r.h, g_theme->background);
    return;
  }
  const bool ok = usable(k), lit = modeLit(k);
  uint32_t fill = lit ? g_theme->keyLit : k.color;
  if (held) fill = lighten(fill, 35);
  d.fillRoundRect(r.x, r.y, r.w, r.h, 10, fill);
  bool ascii = true;
  for (const char *c = k.label; *c; c++) ascii &= (uint8_t)*c < 0x80;
  if (ascii) d.setFont(d.textWidth(k.label, &fonts::DejaVu24) <= r.w - 12 ? &fonts::DejaVu24 : &fonts::DejaVu18);
  else d.setFont(kSymbolFont);
  d.setTextDatum(textdatum_t::middle_center);
  d.setTextColor(ok ? kWhite : kFaint);
  d.drawString(k.label, r.x + r.w / 2, r.y + r.h / 2 + 1);
}

void drawTab(int i, bool held) {
  auto &d = M5.Display;
  const Rect r = tabRect(i);
  const bool on = i == g_page;
  uint32_t fill = on ? g_theme->keyLit : kPanel;
  if (held) fill = lighten(fill, 30);
  d.fillRoundRect(r.x, r.y, r.w, r.h, 8, fill);
  d.setFont(&fonts::DejaVu18);
  d.setTextDatum(textdatum_t::middle_center);
  d.setTextColor(on ? kWhite : kDim);
  d.drawString(kPages[i].name, r.x + r.w / 2, r.y + r.h / 2);
}

void drawFnKeys() {
  M5.Display.startWrite();
  for (int i = 0; i < kFnRows * kFnCols; i++) drawKey(fnRect(i), kPages[g_page].keys[i], g_pressed == i);
  M5.Display.endWrite();
}

void drawPad() {
  M5.Display.startWrite();
  for (int i = 0; i < kPadRows * kPadCols; i++)
    if (i != 1) drawKey(padRect(i), kPad[i], g_pressed == 100 + i);
  M5.Display.endWrite();
}

void drawTabs() {
  M5.Display.startWrite();
  for (int i = 0; i < kPageCount; i++) drawTab(i, g_pressed == 200 + i);
  M5.Display.endWrite();
}

std::string g_barText;

void drawBar(bool force) {
  char modes[64];
  std::string m = rpn::angleName(g_calc.angle());
  m += "   ";
  snprintf(modes, sizeof modes, "%s %d", rpn::fmtName(g_calc.format()), g_calc.digits());
  m += g_calc.format() == rpn::Fmt::All ? "ALL" : modes;
  if (g_calc.baseMode()) m += std::string("   ") + rpn::baseName(g_calc.base());
  if (g_calc.bigStack()) m += "   BIG";
  if (g_calc.statCount()) m += "   n=" + std::to_string(g_calc.statCount());
  if (!force && m == g_barText) return;
  g_barText = m;
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
  d.drawString("Calculator", h.x + h.w + 24, kBarH / 2);
  d.setTextDatum(textdatum_t::middle_right);
  d.setTextColor(kAccent);
  d.drawString(m.c_str(), kW - 24, kBarH / 2);
  d.endWrite();
}

// The display is drawn a line at a time into sprites, so a changing line
// never flickers, and only lines whose text changed are pushed.
LGFX_Sprite g_line(&M5.Display);    // stack levels
LGFX_Sprite g_xLine(&M5.Display);   // X, larger
LGFX_Sprite g_small(&M5.Display);   // LASTx, registers, the message line
constexpr int kLineH = 60, kXH = 92, kSmallH = 40;
constexpr int kMsgY = kDispY + kDispH - kSmallH - 6;
constexpr int kXY = kMsgY - kXH - 6;
constexpr int kMaxLevels = 7;  // above X in the big stack
std::string g_shown[24];       // what each display line shows now

const lgfx::IFont *const kLevelFont = &fonts::DejaVu40;
const lgfx::IFont *const kXFont = &fonts::DejaVu56;

void ensureSprites() {
  if (g_line.width()) return;
  for (auto *s : {&g_line, &g_xLine, &g_small}) s->setPsram(true);
  g_line.createSprite(kDispW, kLineH);
  g_xLine.createSprite(kDispW, kXH);
  g_small.createSprite(kDispW, kSmallH);
}

// Characters of a number that fit beside a label, in that font.
int fitChars(const lgfx::IFont *f, int labelW) {
  g_line.setFont(f);
  const int cw = g_line.textWidth("8");
  return (kDispW - labelW - 28) / (cw ? cw : 1);
}

void pushLine(int slot, LGFX_Sprite &s, int y, const char *label, const std::string &value,
              const lgfx::IFont *font, uint32_t color, uint32_t labelColor = kDim) {
  std::string key = std::string(label) + "\x01" + value;
  key += (char)('0' + (color & 7));
  if (g_shown[slot] == key) return;
  g_shown[slot] = key;
  s.fillSprite(kPanel);
  s.setFont(&fonts::DejaVu24);
  s.setTextDatum(textdatum_t::middle_left);
  s.setTextColor(labelColor);
  s.drawString(label, 14, s.height() / 2);
  s.setFont(font);
  s.setTextDatum(textdatum_t::middle_right);
  s.setTextColor(color);
  s.drawString(value.c_str(), kDispW - 16, s.height() / 2 + 2);
  s.pushSprite(kDispX, y);
}

void pushText(int slot, int y, const std::string &left, uint32_t color, const std::string &right = "",
              uint32_t rightColor = kDim) {
  std::string key = left + "\x01" + right + (char)('0' + (color & 7));
  if (g_shown[slot] == key) return;
  g_shown[slot] = key;
  g_small.fillSprite(kPanel);
  bool ascii = true;
  for (char c : left + right) ascii &= (uint8_t)c < 0x80;
  if (ascii) g_small.setFont(&fonts::DejaVu18);
  else g_small.setFont(&fonts::lgfxJapanGothic_24);
  g_small.setTextDatum(textdatum_t::middle_left);
  g_small.setTextColor(color);
  g_small.drawString(left.c_str(), 14, kSmallH / 2);
  if (!right.empty()) {
    g_small.setTextDatum(textdatum_t::middle_right);
    g_small.setTextColor(rightColor);
    g_small.drawString(right.c_str(), kDispW - 16, kSmallH / 2);
  }
  g_small.pushSprite(kDispX, y);
}

void drawDisplay() {
  ensureSprites();
  const bool big = g_calc.bigStack();
  const int labelW = 60;

  // X.
  std::string x = g_calc.levelText(0, fitChars(kXFont, labelW));
  if (g_calc.entering()) x += "_";
  pushLine(0, g_xLine, kXY, big ? "1:" : "X", x, kXFont, kWhite);

  // Levels above X, bottom up.
  static const char *const kNames[] = {"Y", "Z", "T"};
  const int levels = big ? kMaxLevels : 3;
  const int chars = fitChars(kLevelFont, labelW);
  for (int i = 1; i <= kMaxLevels; i++) {
    const int y = kXY - i * kLineH;
    if (i > levels) {
      if (g_shown[i] != "-") {
        g_shown[i] = "-";
        M5.Display.fillRect(kDispX, y, kDispW, kLineH, kPanel);
      }
      continue;
    }
    char label[8];
    if (big) snprintf(label, sizeof label, "%d:", i + 1);
    else snprintf(label, sizeof label, "%s", kNames[i - 1]);
    const bool there = i < g_calc.depth();
    pushLine(i, g_line, y, label, there ? g_calc.levelText(i, chars) : "", kLevelFont, g_theme->text);
  }

  // Classic stack: LASTx and the registers fill the space above T.
  if (!big) {
    const int top = kDispY + 10;
    pushText(10, top, "LASTx", kDim, g_calc.format(g_calc.lastX(), 24), g_theme->text);
    for (int r = 0; r < 5; r++) {
      const std::string a = g_calc.format(g_calc.reg(r), 13), b = g_calc.format(g_calc.reg(r + 5), 13);
      char t[48];
      snprintf(t, sizeof t, "R%d  %s", r, a.c_str());
      char u[48];
      snprintf(u, sizeof u, "R%d  %s", r + 5, b.c_str());
      pushText(11 + r, top + (r + 1) * kSmallH, t, kFaint, u, kFaint);
    }
  } else {
    for (int r = 10; r < 16; r++) g_shown[r].clear();
  }

  // The message line: an error, a STO/RCL waiting for its register, or hints.
  if (g_calc.error() != rpn::Err::None) {
    pushText(20, kMsgY, g_calc.errorText(), kErr);
  } else if (g_pending != Act::None) {
    std::string p = g_pending == Act::Sto ? "STO" : "RCL";
    if (g_pendingArith != Op::None) p += rpn::opLabel(g_pendingArith);
    pushText(20, kMsgY, p + " _   register 0-9, Esc cancels", kPrompt);
  } else if (g_kb && g_kb->present()) {
    pushText(20, kMsgY, "keys: s c t trig, q sqrt, ^ power, n CHS, x swap, u undo", kFaint);
  } else {
    pushText(20, kMsgY, g_calc.canUndo() ? "UNDO takes back the last step" : "", kFaint);
  }
}

void drawAll() {
  auto &d = M5.Display;
  for (auto &s : g_shown) s.clear();
  d.startWrite();
  d.fillScreen(g_theme->background);
  d.fillRoundRect(kDispX - 4, kDispY - 4, kDispW + 8, kDispH + 8, 14, kPanel);
  d.endWrite();
  drawBar(true);
  drawTabs();
  drawFnKeys();
  drawPad();
  drawDisplay();
}

// ------------------------------------------------------------------ input --

void setPage(int p) {
  p = (p % kPageCount + kPageCount) % kPageCount;
  if (p == g_page) return;
  g_page = p;
  changed();
  drawTabs();
  drawFnKeys();
}

// After anything that may change modes: keys light or fade with them.
void afterKey(bool modesMayChange) {
  if (modesMayChange) {
    drawFnKeys();
    drawPad();
  }
  drawBar(false);
  drawDisplay();
  changed();
}

void doKey(const Key &k) {
  if (k.act == Act::None) return;
  // A register digit for a waiting STO/RCL.
  if (g_pending != Act::None) {
    const Act p = g_pending;
    g_pending = Act::None;
    if (k.act == Act::Digit && k.arg < 10) {
      if (p == Act::Sto) g_calc.sto(k.arg, g_pendingArith);
      else g_calc.rcl(k.arg, g_pendingArith);
      afterKey(true);
      return;
    }
    drawFnKeys();
    if (k.act == Act::Sto || k.act == Act::Rcl) {
      // Another STO/RCL key: switch to it (below) rather than cancel.
    } else {
      drawDisplay();
      return;  // anything else cancels
    }
  }
  bool modes = false;
  switch (k.act) {
    case Act::Digit:
      if (!usable(k)) return;
      g_calc.digit(k.arg);
      break;
    case Act::Point: g_calc.point(); break;
    case Act::Eex: g_calc.eex(); break;
    case Act::Chs: g_calc.chs(); break;
    case Act::Back: g_calc.backspace(); break;
    case Act::Op:
      g_calc.apply((Op)k.arg);
      modes = (Op)k.arg == Op::Undo;  // undo can switch the base back
      break;
    case Act::Sto:
    case Act::Rcl:
      g_calc.clearError();
      g_pending = k.act;
      g_pendingArith = (Op)k.arg;
      drawFnKeys();
      drawDisplay();
      return;
    case Act::Angle: g_calc.setAngle((rpn::Angle)k.arg); modes = true; break;
    case Act::Base: g_calc.setBase((rpn::Base)k.arg); modes = true; break;
    case Act::Fmt: {
      int digits = g_calc.digits();
      if ((rpn::Fmt)k.arg != rpn::Fmt::All && g_calc.format() == rpn::Fmt::All && digits == 0) digits = 4;
      g_calc.setFormat((rpn::Fmt)k.arg, digits);
      modes = true;
      break;
    }
    case Act::DigitsDown: g_calc.setFormat(g_calc.format(), g_calc.digits() > 0 ? g_calc.digits() - 1 : 0); break;
    case Act::DigitsUp:
      g_calc.setFormat(g_calc.format(), g_calc.digits() < rpn::kMaxDigits ? g_calc.digits() + 1 : rpn::kMaxDigits);
      break;
    case Act::BigStack: g_calc.setBigStack(!g_calc.bigStack()); modes = true; break;
    case Act::Group: g_calc.setGrouping(!g_calc.grouping()); modes = true; break;
    case Act::ClearAll: g_calc.reset(); modes = true; break;
    default: return;
  }
  afterKey(modes);
}

void doOp(Op op) { doKey({"", Act::Op, (int)op, 0}); }
void doAct(Act a, int arg = 0) { doKey({"", a, arg, 0}); }

void typed(const KeyText &t) {
  if (t.special) {
    switch (t.special) {
      case key::Enter: doOp(Op::Enter); break;
      case key::Backspace: doAct(Act::Back); break;
      case key::Escape:
        if (g_pending != Act::None) {
          g_pending = Act::None;
          drawFnKeys();
          drawDisplay();
        } else {
          doOp(Op::Clx);
        }
        break;
      case key::Left: setPage(g_page - 1); break;
      case key::Right: setPage(g_page + 1); break;
      case key::Delete: doOp(Op::Clx); break;
    }
    return;
  }
  const char c = t.ch;
  if (t.ctrl) {
    if (c == 'z' || c == 'Z') doOp(Op::Undo);
    return;
  }
  if (c >= '0' && c <= '9') return doAct(Act::Digit, c - '0');
  if (g_calc.base() == rpn::Base::Hex) {
    if (c >= 'a' && c <= 'f') return doAct(Act::Digit, 10 + c - 'a');
    if (c >= 'A' && c <= 'F') return doAct(Act::Digit, 10 + c - 'A');
  }
  switch (c) {
    case '.': doAct(Act::Point); break;
    case 'e': case 'E': doAct(Act::Eex); break;
    case 'n': case '_': doAct(Act::Chs); break;
    case '+': doOp(Op::Add); break;
    case '-': doOp(Op::Sub); break;
    case '*': doOp(Op::Mul); break;
    case '/': doOp(Op::Div); break;
    case '^': doOp(Op::Pow); break;
    case 'x': doOp(Op::Swap); break;
    case 'r': doOp(Op::RollDown); break;
    case 'R': doOp(Op::RollUp); break;
    case 'u': doOp(Op::Undo); break;
    case 'w': doOp(Op::LastX); break;
    case 's': doOp(Op::Sin); break;
    case 'c': doOp(Op::Cos); break;
    case 't': doOp(Op::Tan); break;
    case 'S': doOp(Op::Asin); break;
    case 'C': doOp(Op::Acos); break;
    case 'T': doOp(Op::Atan); break;
    case 'q': doOp(Op::Sqrt); break;
    case 'i': doOp(Op::Inv); break;
    case 'l': doOp(Op::Ln); break;
    case 'L': doOp(Op::Log); break;
    case 'p': doOp(Op::Pi); break;
    case '!': doOp(Op::Fact); break;
    case '%': doOp(Op::Pct); break;
    case ' ': doOp(Op::Enter); break;
  }
}

// What is at (x, y): see g_pressed.
int hitTest(int x, int y) {
  for (int i = 0; i < kPageCount; i++)
    if (tabRect(i).contains(x, y)) return 200 + i;
  for (int i = 0; i < kFnRows * kFnCols; i++)
    if (fnRect(i).contains(x, y)) return kPages[g_page].keys[i].act == Act::None ? -1 : i;
  for (int i = 0; i < kPadRows * kPadCols; i++)
    if (i != 1 && padRect(i).contains(x, y)) return 100 + i;
  return -1;
}

void drawPressed(int which, bool held) {
  M5.Display.startWrite();
  if (which >= 200) drawTab(which - 200, held);
  else if (which >= 100) drawKey(padRect(which - 100), kPad[which - 100], held);
  else if (which >= 0) drawKey(fnRect(which), kPages[g_page].keys[which], held);
  M5.Display.endWrite();
}

// A key acts as it goes down (the calculator feels quicker that way).
void press(int which) {
  if (which >= 200) setPage(which - 200);
  else if (which >= 100) doKey(kPad[which - 100]);
  else if (which >= 0) doKey(kPages[g_page].keys[which]);
}

void handleTouch() {
  for (int i = 0; i < M5.Touch.getCount(); i++) {
    const auto &t = M5.Touch.getDetail(i);
    if (t.wasPressed()) {
      if (homeButton().contains(t.x, t.y)) {
        appGoHome();
        return;
      }
      if (g_pressed >= 0) continue;  // one key at a time
      const int which = hitTest(t.x, t.y);
      if (which < 0) continue;
      g_pressed = which;
      g_pressId = t.id;
      press(which);
      if (g_pressed == which) drawPressed(which, true);
    } else if (t.wasReleased() && t.id == g_pressId && g_pressed >= 0) {
      const int was = g_pressed;
      g_pressed = -1;
      g_pressId = -1;
      drawPressed(was, false);
    }
  }
  if (g_pressed >= 0 && M5.Touch.getCount() == 0) {
    const int was = g_pressed;
    g_pressed = -1;
    drawPressed(was, false);
  }
}

}  // namespace

void calcBegin(Tab5Keyboard &kb, const Theme &theme) {
  g_kb = &kb;
  g_theme = &theme;
  if (!g_loaded) {
    g_loaded = true;
    g_calc.seed(esp_random() | (uint64_t)esp_random() << 32);
    load();
  }
  g_pressed = -1;
  g_pending = Act::None;
  drawAll();
}

void calcEnd() {
  g_calc.commitEntry();
  save();
}

void calcLoop(uint32_t now) {
  handleTouch();
  KeyEvent e;
  KeyText t;
  while (g_kb->next(&e))
    if (KeyText::from(e, &t)) typed(t);
  static bool kbWas = false;
  if (g_kb->present() != kbWas) {
    kbWas = g_kb->present();
    drawDisplay();  // the hint line
  }
  // Saved from loop(), a few seconds after the last change.
  if (g_dirty && now - g_changedAt > 3000) save();
}

void calcType(char c) {
  KeyText t;
  if (c == '\n' || c == '\r') t.special = key::Enter;
  else if (c == 27) t.special = key::Escape;
  else if (c == 8 || c == 127) t.special = key::Backspace;
  else if (c == '<') t.special = key::Left;
  else if (c == '>') t.special = key::Right;
  else t.ch = c;
  typed(t);
}

void calcTap(int x, int y) {
  if (homeButton().contains(x, y)) {
    appGoHome();
    return;
  }
  const int which = hitTest(x, y);
  press(which);
}

}  // namespace mt
