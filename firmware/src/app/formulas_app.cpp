// The Formulas app: formulas.cpp has the library and the solver, this file draws
// them and takes input.
//
// Browse screen: the groups down the left, the formulas of the chosen group on the
// right, each with its equation. Quick (first) holds favourites, recent formulas and
// the everyday ones. Typing anywhere searches the whole library; Enter opens the top
// result. Without the clip-on keyboard, tapping the search box brings up a keyboard.
//
// Sheet screen: the diagram and the equation on the left, a row per variable on the
// right. Fill in what you know and the blank rows are solved; once enough is filled
// in, typing a new value recomputes the oldest entered one, and a row's = button
// makes it the one to solve for. Fields take "4.7k", "100n", "3/8", "5 ft 3 in",
// "2.2 uF". Below, the focused variable's help and its value in the common units of
// its kind: tap one to show the row in that unit. A solved row's equation card shows
// the formula rearranged for it. Tap the unit button for all units and handy values.
// Keys: Tab, Up/Down or Enter move between rows, Esc goes back.
//
// Everything is drawn into one full-screen sprite and pushed when something changes.
// Favourites and recent formulas are kept in NVS ("formulas").
#include "formulas_app.h"

#include <M5Unified.h>
#include <Preferences.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "formulas.h"

using namespace t5m;
namespace f = mt::formulas;
namespace u = mt::units;

namespace mt {
namespace {

constexpr int kW = 1280, kH = 720, kBarH = 64;
constexpr uint32_t kBarBg = 0x080A0C, kDim = 0x8A949E, kFaint = 0x56606A, kWhite = 0xFFFFFF;
constexpr uint32_t kPanel = 0x161B21, kPanel2 = 0x1E252D, kAccent = 0x7FB8FF, kAnswer = 0x9FE6A0, kErr = 0xFF8A7A;
constexpr uint32_t kChip = 0x232A32, kWarm = 0xFFB86B, kStar = 0xFFD45E, kDigitKey = 0x3A424C, kFnKey = 0x2C333B;

Tab5Keyboard *g_kb = nullptr;
const Theme *g_theme = nullptr;
LGFX_Sprite g_fb(&M5.Display);
bool g_fbReady = false;
bool g_dirty = true;     // the screen needs redrawing
bool g_kbPresent = false;

enum class Screen : uint8_t { Browse, Sheet };
Screen g_screen = Screen::Browse;

// Browse.
constexpr int kQuick = -1;            // the Quick entry in the rail
int g_group = kQuick;
std::string g_query;
bool g_queryOld = false;              // back from a formula: the next letter starts afresh
bool g_searchKeys = false;            // on-screen keyboard open for the search box
int g_list[256];
int g_listCount = 0;
int g_sel = 0;                        // highlighted row
int g_scroll = 0;                     // pixels
Keypad g_keypad;
KeypadRenderer g_keypadDraw;
const KeyPage *const kPages[] = {&pages::Qwerty};

// Sheet.
f::Sheet g_sheet;
const f::Formula *g_f = nullptr;
int g_fIndex = -1;
int g_focus = 0;
bool g_editing = false;
std::string g_edit;
std::string g_message;                // a parse error, until the next change
int g_unit[f::kMaxVars];              // the unit each row is shown in
bool g_unitChosen[f::kMaxVars];       // chosen by hand, so a solve won't rescale it
int g_equation = 0;                   // the stored equation shown
int g_picker = -1;                    // the row whose unit picker is open

// Saved.
std::vector<std::string> g_favs, g_recent;
bool g_saveDue = false;

// Touch.
bool g_down = false, g_dragging = false;
int g_downX = 0, g_downY = 0, g_downScroll = 0;

// ------------------------------------------------------------------ text --

// Three sizes only: each one linked in costs hundreds of KB of glyphs, and the
// Calculator and Terminal already use 24 and 28.
const lgfx::IFont *jpFont(int size) {
  if (size >= 28) return &fonts::lgfxJapanGothic_28;
  if (size >= 24) return &fonts::lgfxJapanGothic_24;
  return &fonts::lgfxJapanGothic_16;
}

int smaller(int) { return 16; }

bool ascii(const char *s) {
  for (; *s; s++)
    if ((uint8_t)*s >= 0x80) return false;
  return true;
}

// ASCII reads better in DejaVu; anything with Greek needs the Japanese font.
void useFont(const char *s, int size) {
  if (!ascii(s)) { g_fb.setFont(jpFont(size)); return; }
  if (size >= 40) g_fb.setFont(&fonts::DejaVu40);
  else if (size >= 24) g_fb.setFont(&fonts::DejaVu24);
  else if (size >= 18) g_fb.setFont(&fonts::DejaVu18);
  else g_fb.setFont(&fonts::DejaVu12);
}

void text(const char *s, int x, int y, int size, uint32_t color, textdatum_t datum = textdatum_t::middle_left) {
  useFont(s, size);
  g_fb.setTextDatum(datum);
  g_fb.setTextColor(color);
  g_fb.drawString(s, x, y);
}

int textWidth(const char *s, int size) {
  useFont(s, size);
  return g_fb.textWidth(s);
}

// Cut to fit a width, with "..." at the end.
std::string fit(const char *s, int size, int w) {
  std::string t = s;
  if (textWidth(t.c_str(), size) <= w) return t;
  while (!t.empty() && textWidth((t + "...").c_str(), size) > w) {
    t.pop_back();
    while (!t.empty() && ((uint8_t)t.back() & 0xC0) == 0x80) t.pop_back();  // a whole UTF-8 character
    if (!t.empty() && (uint8_t)t.back() >= 0xC0) t.pop_back();
  }
  return t + "...";
}

// Word wrap into at most maxLines lines; returns the height used.
int wrap(const char *s, int x, int y, int w, int size, uint32_t color, int maxLines) {
  const int lineH = size + 7;
  std::string line, word;
  int lines = 0;
  auto flush = [&](bool last) {
    text((last ? fit(line.c_str(), size, w) : line).c_str(), x, y + lines * lineH, size, color, textdatum_t::top_left);
    lines++;
    line.clear();
  };
  for (const char *p = s;; p++) {
    if (*p && *p != ' ') { word += *p; continue; }
    const std::string joined = line.empty() ? word : line + " " + word;
    if (textWidth(joined.c_str(), size) > w && !line.empty()) {
      if (lines == maxLines - 1) { line = joined; break; }
      flush(false);
      line = word;
    } else {
      line = joined;
    }
    word.clear();
    if (!*p) break;
  }
  if (!line.empty() && lines < maxLines) flush(true);
  return lines * lineH;
}

// Unit symbols are ASCII in the tables; the screen can do better.
std::string pretty(const char *sym) {
  std::string s = sym;
  size_t p;
  while ((p = s.find("ohm")) != std::string::npos) s.replace(p, 3, "Ω");
  if (s.size() > 1 && s[0] == 'u' && (isupper((uint8_t)s[1]) || s.compare(1, 2, "Ω") == 0 || s[1] == 'm' || s[1] == 's' || s[1] == 'g' || s[1] == 'T'))
    s.replace(0, 1, "μ");
  if (s == "deg") s = "°";
  return s;
}

// A label with _{ } subscripts: returns the width; draws when draw is set.
int label(const char *s, int x, int base, int size, uint32_t color, bool draw = true) {
  const int x0 = x;
  std::string run;
  auto put = [&](const std::string &t, int sz, int dy) {
    if (t.empty()) return;
    g_fb.setFont(jpFont(sz));
    if (draw) {
      g_fb.setTextDatum(textdatum_t::baseline_left);
      g_fb.setTextColor(color);
      g_fb.drawString(t.c_str(), x, base + dy);
    }
    x += g_fb.textWidth(t.c_str());
  };
  for (const char *p = s; *p; p++) {
    if (p[0] == '_' && p[1] == '{') {
      put(run, size, 0);
      run.clear();
      const char *e = strchr(p, '}');
      if (!e) break;
      put(std::string(p + 2, e - p - 2), smaller(size), size / 4);
      p = e;
    } else {
      run += *p;
    }
  }
  put(run, size, 0);
  return x - x0;
}

// ------------------------------------------------- equation typesetting --

struct Box {
  int w = 0, up = 0, down = 0;  // above and below the baseline
};

int precedence(const f::Node *n) {
  switch (n->op) {
    case f::Node::Add: case f::Node::Sub: return 1;
    case f::Node::Mul: case f::Node::Neg: return 2;
    case f::Node::Div: return 5;  // stacked: never needs brackets
    case f::Node::Pow: return 3;
    default: return 5;
  }
}

std::string numberText(double v) {
  char b[24];
  snprintf(b, sizeof b, "%.6g", v);
  return b;
}

bool isHalf(const f::Node *n) { return n->op == f::Node::Num && n->value == 0.5; }
bool isSqrt(const f::Node *n) {
  return (n->op == f::Node::Func && !strcmp(n->name, "sqrt")) || (n->op == f::Node::Pow && isHalf(n->b));
}

Box layout(const f::Node *n, int sz, bool draw, int x, int base, uint32_t col);

// A child, in brackets when its precedence asks for it.
Box bracket(const f::Node *n, bool need, int sz, bool draw, int x, int base, uint32_t col) {
  if (!need) return layout(n, sz, draw, x, base, col);
  Box in = layout(n, sz, false, 0, 0, col);
  const int bw = sz / 3 + 2;
  if (draw) {
    const int top = base - in.up, bot = base + in.down, r = x + bw + in.w + 4;
    for (int t = 0; t < 2; t++) {
      g_fb.drawLine(x + bw - 2 + t, top, x + 2 + t, top + (bot - top) / 4, col);
      g_fb.drawLine(x + 2 + t, top + (bot - top) / 4, x + 2 + t, bot - (bot - top) / 4, col);
      g_fb.drawLine(x + 2 + t, bot - (bot - top) / 4, x + bw - 2 + t, bot, col);
      g_fb.drawLine(r - t, top, r + bw - 4 - t, top + (bot - top) / 4, col);
      g_fb.drawLine(r + bw - 4 - t, top + (bot - top) / 4, r + bw - 4 - t, bot - (bot - top) / 4, col);
      g_fb.drawLine(r + bw - 4 - t, bot - (bot - top) / 4, r - t, bot, col);
    }
    layout(n, sz, true, x + bw + 2, base, col);
  }
  in.w += 2 * bw + 4;
  return in;
}

Box join(Box a, Box b) { return {a.w + b.w, std::max(a.up, b.up), std::max(a.down, b.down)}; }

Box glyph(const char *s, int sz, bool draw, int x, int base, uint32_t col) {
  Box b;
  b.w = label(s, x, base, sz, col, draw);
  b.up = sz * 4 / 5;
  b.down = sz / 4;
  return b;
}

Box layout(const f::Node *n, int sz, bool draw, int x, int base, uint32_t col) {
  using N = f::Node;
  auto op = [&](const char *s, Box left, Box right, const N *a, const N *b, bool ra, bool rb) {
    const int gap = sz / 4, ow = label(s, 0, 0, sz, col, false);
    if (draw) {
      bracket(a, ra, sz, true, x, base, col);
      label(s, x + left.w + gap, base, sz, col);
      bracket(b, rb, sz, true, x + left.w + gap * 2 + ow, base, col);
    }
    Box r = join(left, right);
    r.w += gap * 2 + ow;
    return r;
  };
  switch (n->op) {
    case N::Num: return glyph(numberText(n->value).c_str(), sz, draw, x, base, col);
    case N::Var: return glyph(g_f && n->var >= 0 && n->var < g_f->varCount ? g_f->vars[n->var].label : "?", sz, draw, x, base, col);
    case N::Const: return glyph(n->name, sz, draw, x, base, col);
    case N::Add: case N::Sub: {
      const bool rb = n->op == N::Sub && precedence(n->b) <= 1;
      Box l = layout(n->a, sz, false, 0, 0, col), r = bracket(n->b, rb, sz, false, 0, 0, col);
      return op(n->op == N::Add ? "+" : "-", l, r, n->a, n->b, false, rb);
    }
    case N::Mul: {
      const bool ra = precedence(n->a) <= 1, rb = precedence(n->b) <= 1 || n->b->op == N::Neg;
      Box l = bracket(n->a, ra, sz, false, 0, 0, col), r = bracket(n->b, rb, sz, false, 0, 0, col);
      // 2πr runs together; between two letters, a raised dot.
      const bool tight = n->a->op == N::Num && n->b->op != N::Num;
      const int gap = tight ? 2 : sz / 3;
      if (draw) {
        bracket(n->a, ra, sz, true, x, base, col);
        if (!tight) g_fb.fillCircle(x + l.w + gap / 2, base - sz * 3 / 10, std::max(2, sz / 12), col);
        bracket(n->b, rb, sz, true, x + l.w + gap, base, col);
      }
      Box b = join(l, r);
      b.w += gap;
      return b;
    }
    case N::Div: {
      // The rearranged trees nest fractions: (a/b)/c reads better as a/(bc),
      // and a/(b/c) as ac/b.
      if (n->a->op == N::Div) {
        N den, top;
        den.op = N::Mul; den.a = n->a->b; den.b = n->b;
        top.op = N::Div; top.a = n->a->a; top.b = &den;
        return layout(&top, sz, draw, x, base, col);
      }
      if (n->b->op == N::Div) {
        N num, top;
        num.op = N::Mul; num.a = n->a; num.b = n->b->b;
        top.op = N::Div; top.a = &num; top.b = n->b->a;
        return layout(&top, sz, draw, x, base, col);
      }
      const int s2 = sz > 24 ? sz - 4 : sz;
      Box t = layout(n->a, s2, false, 0, 0, col), d = layout(n->b, s2, false, 0, 0, col);
      const int w = std::max(t.w, d.w) + 8, axis = sz * 3 / 10;
      if (draw) {
        layout(n->a, s2, true, x + (w - t.w) / 2, base - axis - 5 - t.down, col);
        g_fb.fillRect(x, base - axis, w, 2, col);
        layout(n->b, s2, true, x + (w - d.w) / 2, base - axis + 6 + d.up, col);
      }
      return {w, axis + 5 + t.down + t.up, d.up + d.down + 6 - axis};
    }
    case N::Pow: {
      if (isHalf(n->b)) break;
      const bool rb = precedence(n->a) <= 3;
      Box b = bracket(n->a, rb, sz, false, 0, 0, col);
      const int s2 = smaller(sz);
      Box e = layout(n->b, s2, false, 0, 0, col);
      const int lift = sz * 2 / 5 + e.down;
      if (draw) {
        bracket(n->a, rb, sz, true, x, base, col);
        layout(n->b, s2, true, x + b.w + 1, base - lift, col);
      }
      return {b.w + e.w + 1, std::max(b.up, lift + e.up), b.down};
    }
    case N::Neg: {
      Box m = glyph("-", sz, draw, x, base, col);
      const bool r = precedence(n->a) <= 1;
      Box v = bracket(n->a, r, sz, draw, x + m.w, base, col);
      return join(m, v);
    }
    case N::Func: {
      if (isSqrt(n)) break;
      if (!strcmp(n->name, "abs")) {
        Box in = layout(n->a, sz, false, 0, 0, col);
        if (draw) {
          g_fb.fillRect(x + 2, base - in.up, 2, in.up + in.down, col);
          layout(n->a, sz, true, x + 8, base, col);
          g_fb.fillRect(x + in.w + 12, base - in.up, 2, in.up + in.down, col);
        }
        in.w += 16;
        return in;
      }
      if (!strcmp(n->name, "exp")) {
        Box e = glyph("e", sz, draw, x, base, col);
        const int s2 = smaller(sz);
        Box p = layout(n->a, s2, false, 0, 0, col);
        const int lift = sz * 2 / 5 + p.down;
        if (draw) layout(n->a, s2, true, x + e.w + 1, base - lift, col);
        return {e.w + p.w + 1, std::max(e.up, lift + p.up), e.down};
      }
      Box name = glyph(n->name, sz, draw, x, base, col);
      name.w += 2;
      if (n->b) {  // two arguments: name(a, b)
        Box in = layout(n->a, sz, false, 0, 0, col), c = glyph(", ", sz, false, 0, 0, col), in2 = layout(n->b, sz, false, 0, 0, col);
        Box open = glyph("(", sz, draw, x + name.w, base, col);
        int px = x + name.w + open.w;
        if (draw) {
          layout(n->a, sz, true, px, base, col);
          glyph(", ", sz, true, px + in.w, base, col);
          layout(n->b, sz, true, px + in.w + c.w, base, col);
          glyph(")", sz, true, px + in.w + c.w + in2.w, base, col);
        }
        Box all = join(join(name, open), join(join(in, c), in2));
        all.w += open.w;
        return all;
      }
      Box arg = bracket(n->a, true, sz, draw, x + name.w, base, col);
      return join(name, arg);
    }
    default: break;
  }
  // The square root sign, drawn: isSqrt() cases land here.
  const N *in = n->a;
  Box b = layout(in, sz, false, 0, 0, col);
  const int lead = sz / 2 + 4, top = base - b.up - 5;
  if (draw) {
    for (int t = 0; t < 2; t++) {
      g_fb.drawLine(x + t, base - sz / 4, x + sz / 6 + t, base - sz / 4 - 3, col);
      g_fb.drawLine(x + sz / 6 + t, base - sz / 4 - 3, x + sz / 3 + t, base + b.down, col);
      g_fb.drawLine(x + sz / 3 + t, base + b.down, x + lead - 2 + t, top, col);
    }
    g_fb.fillRect(x + lead - 2, top, b.w + 6, 2, col);
    layout(in, sz, true, x + lead + 1, base, col);
  }
  return {b.w + lead + 5, b.up + 7, b.down};
}

// lhs = rhs in a box, shrinking until it fits. Centred when centre is set.
void equationAt(const f::Node *lhs, const f::Node *rhs, int x, int y, int w, int h, uint32_t col, bool centre, int start = 28) {
  for (int sz = start;; sz -= 4) {
    Box l = layout(lhs, sz, false, 0, 0, col), r = layout(rhs, sz, false, 0, 0, col);
    const int eq = sz, total = l.w + r.w + eq, up = std::max(l.up, r.up), down = std::max(l.down, r.down);
    if ((total <= w && up + down <= h) || sz <= 12) {
      const int x0 = centre ? x + (w - total) / 2 : x + w - total;
      const int base = y + (h - up - down) / 2 + up;
      layout(lhs, sz, true, x0, base, col);
      label("=", x0 + l.w + sz / 4, base, sz, col);
      layout(rhs, sz, true, x0 + l.w + eq, base, col);
      return;
    }
  }
}

// ------------------------------------------------------------- diagrams --

// A scaled canvas for one drawing.
struct Canvas {
  int ox, oy;
  float s;
  uint32_t ink[4];
  uint32_t c;
  int X(float x) const { return ox + (int)(x * s); }
  int Y(float y) const { return oy + (int)(y * s); }
};

void thick(const Canvas &v, float x1, float y1, float x2, float y2) {
  const int a = v.X(x1), b = v.Y(y1), c = v.X(x2), d = v.Y(y2);
  g_fb.drawLine(a, b, c, d, v.c);
  if (std::fabs(x2 - x1) >= std::fabs(y2 - y1)) g_fb.drawLine(a, b + 1, c, d + 1, v.c);
  else g_fb.drawLine(a + 1, b, c + 1, d, v.c);
}

void head(const Canvas &v, float x, float y, float dx, float dy) {
  const float n = std::sqrt(dx * dx + dy * dy);
  if (n < 0.01f) return;
  dx /= n; dy /= n;
  const float L = 11, W = 4.5f;
  g_fb.fillTriangle(v.X(x), v.Y(y), v.X(x - dx * L - dy * W), v.Y(y - dy * L + dx * W),
                    v.X(x - dx * L + dy * W), v.Y(y - dy * L - dx * W), v.c);
}

// Along a part: position t of the way from 1 to 2, n to the left of that line.
struct Along {
  float x1, y1, ux, uy, len;
  Along(float a, float b, float c, float d) : x1(a), y1(b) {
    len = std::sqrt((c - a) * (c - a) + (d - b) * (d - b));
    ux = len > 0 ? (c - a) / len : 1;
    uy = len > 0 ? (d - b) / len : 0;
  }
  float X(float t, float n) const { return x1 + ux * t - uy * n; }
  float Y(float t, float n) const { return y1 + uy * t + ux * n; }
};

void part(const Canvas &v, char kind, float x1, float y1, float x2, float y2) {
  Along a(x1, y1, x2, y2);
  const float body = std::min(a.len * 0.6f, 56.0f), t0 = (a.len - body) / 2, t1 = t0 + body, mid = a.len / 2;
  auto seg = [&](float ta, float na, float tb, float nb) { thick(v, a.X(ta, na), a.Y(ta, na), a.X(tb, nb), a.Y(tb, nb)); };
  auto leads = [&](float p, float q) { seg(0, 0, p, 0); seg(q, 0, a.len, 0); };
  switch (kind) {
    case 'r': {
      leads(t0, t1);
      const int n = 6;
      float pt = t0, pn = 0;
      for (int i = 0; i < n; i++) {
        const float t = t0 + body * (i + 0.5f) / n, m = (i & 1) ? -9 : 9;
        seg(pt, pn, t, m);
        pt = t; pn = m;
      }
      seg(pt, pn, t1, 0);
      break;
    }
    case 'c':
      leads(mid - 5, mid + 5);
      seg(mid - 5, -16, mid - 5, 16);
      seg(mid + 5, -16, mid + 5, 16);
      break;
    case 'l': {
      leads(t0, t1);
      const int n = 4;
      const float r = body / n / 2;
      for (int i = 0; i < n; i++) {
        float pt = t0 + i * 2 * r, pn = 0;
        for (int k = 1; k <= 10; k++) {
          const float an = 3.14159f * k / 10;
          const float t = t0 + i * 2 * r + r - r * std::cos(an), m = -r * 1.3f * std::sin(an);
          seg(pt, pn, t, m);
          pt = t; pn = m;
        }
      }
      break;
    }
    case 'b':
      leads(mid - 4, mid + 4);
      seg(mid - 4, -8, mid - 4, 8);
      seg(mid + 4, -18, mid + 4, 18);
      {
        const float t = mid + 16, n = -20;  // the + sign
        seg(t - 4, n, t + 4, n);
        seg(t, n - 4, t, n + 4);
      }
      break;
    case 's': {
      const float r = 20;
      leads(mid - r, mid + r);
      g_fb.drawCircle(v.X(a.X(mid, 0)), v.Y(a.Y(mid, 0)), (int)(r * v.s), v.c);
      g_fb.drawCircle(v.X(a.X(mid, 0)), v.Y(a.Y(mid, 0)), (int)(r * v.s) - 1, v.c);
      float pt = mid - 12, pn = 0;
      for (int k = 1; k <= 12; k++) {
        const float t = mid - 12 + 2 * k, m = 7 * std::sin(k * 3.14159f / 6);
        seg(pt, pn, t, m);
        pt = t; pn = m;
      }
      break;
    }
    case 'd': case 'e': {
      leads(mid - 10, mid + 10);
      g_fb.fillTriangle(v.X(a.X(mid - 10, -12)), v.Y(a.Y(mid - 10, -12)), v.X(a.X(mid - 10, 12)), v.Y(a.Y(mid - 10, 12)),
                        v.X(a.X(mid + 10, 0)), v.Y(a.Y(mid + 10, 0)), v.c);
      seg(mid + 10, -12, mid + 10, 12);
      if (kind == 'e') {
        for (int i = 0; i < 2; i++) {
          const float t = mid - 6 + i * 11;
          const float xa = a.X(t, -16), ya = a.Y(t, -16), xb = a.X(t + 9, -28), yb = a.Y(t + 9, -28);
          thick(v, xa, ya, xb, yb);
          head(v, xb, yb, xb - xa, yb - ya);
        }
      }
      break;
    }
  }
}

void drawDiagram(const char *script, int x, int y, int w, int h, uint32_t accent) {
  Canvas v;
  v.s = std::min(w / 400.0f, h / 240.0f);
  v.ox = x + (int)((w - 400 * v.s) / 2);
  v.oy = y + (int)((h - 240 * v.s) / 2);
  v.ink[0] = 0xD8DEE4; v.ink[1] = accent; v.ink[2] = kWarm; v.ink[3] = 0x6B7680;
  v.c = v.ink[0];
  const int labelSize = v.s >= 1 ? 24 : 16, smallSize = v.s >= 1 ? 20 : 12;

  const char *p = script;
  while (*p) {
    while (*p == ' ' || *p == ';') p++;
    if (!*p) break;
    const char cmd = *p++;
    const char *end = strchr(p, ';');
    if (!end) end = p + strlen(p);
    float a[32];
    int n = 0;
    const char *q = p;
    if (cmd == 'T' || cmd == 't') {
      for (int i = 0; i < 2; i++) {
        char *e;
        a[i] = strtof(q, &e);
        q = e;
      }
      while (*q == ' ') q++;
      const std::string s(q, end - q);
      const int sz = cmd == 'T' ? labelSize : smallSize;
      const int lw = label(s.c_str(), 0, 0, sz, v.c, false);
      label(s.c_str(), v.X(a[0]) - lw / 2, v.Y(a[1]) + sz * 3 / 8, sz, v.c);
      p = end;
      continue;
    }
    while (q < end && n < 32) {
      char *e;
      const float val = strtof(q, &e);
      if (e == q) break;
      a[n++] = val;
      q = e;
    }
    switch (cmd) {
      case 'k': if (n >= 1 && a[0] >= 0 && a[0] < 4) v.c = v.ink[(int)a[0]]; break;
      case 'L': thick(v, a[0], a[1], a[2], a[3]); break;
      case 'A':
        thick(v, a[0], a[1], a[2], a[3]);
        head(v, a[2], a[3], a[2] - a[0], a[3] - a[1]);
        break;
      case 'D':
        g_fb.drawLine(v.X(a[0]), v.Y(a[1]), v.X(a[2]), v.Y(a[3]), v.c);
        head(v, a[2], a[3], a[2] - a[0], a[3] - a[1]);
        head(v, a[0], a[1], a[0] - a[2], a[1] - a[3]);
        break;
      case 'B': g_fb.drawRect(v.X(a[0]), v.Y(a[1]), (int)(a[2] * v.s), (int)(a[3] * v.s), v.c);
                g_fb.drawRect(v.X(a[0]) + 1, v.Y(a[1]) + 1, (int)(a[2] * v.s) - 2, (int)(a[3] * v.s) - 2, v.c); break;
      case 'F': g_fb.fillRect(v.X(a[0]), v.Y(a[1]), (int)(a[2] * v.s), (int)(a[3] * v.s), v.c); break;
      case 'C': g_fb.drawCircle(v.X(a[0]), v.Y(a[1]), (int)(a[2] * v.s), v.c);
                g_fb.drawCircle(v.X(a[0]), v.Y(a[1]), (int)(a[2] * v.s) - 1, v.c); break;
      case 'O': g_fb.fillCircle(v.X(a[0]), v.Y(a[1]), (int)(a[2] * v.s), v.c); break;
      case 'W': {
        const int steps = 32;
        float px = 0, py = 0;
        for (int i = 0; i <= steps; i++) {
          const float an = (a[3] + (a[4] - a[3]) * i / steps) * 3.14159f / 180;
          const float cx = a[0] + a[2] * std::cos(an), cy = a[1] - a[2] * std::sin(an);
          if (i) thick(v, px, py, cx, cy);
          px = cx; py = cy;
        }
        break;
      }
      case 'P':
        for (int i = 2; i + 3 < n; i += 2)
          g_fb.fillTriangle(v.X(a[0]), v.Y(a[1]), v.X(a[i]), v.Y(a[i + 1]), v.X(a[i + 2]), v.Y(a[i + 3]), v.c);
        break;
      case 'Q': {
        thick(v, a[0], a[1], a[2], a[3]);
        Along l(a[0], a[1], a[2], a[3]);
        for (float t = 6; t < l.len; t += 14) thick(v, l.X(t, 0), l.Y(t, 0), l.X(t - 8, 10), l.Y(t - 8, 10));
        break;
      }
      case 'M': {
        Along l(a[0], a[1], a[2], a[3]);
        const int coils = 7;
        const float lead = std::min(14.0f, l.len / 6), span = l.len - 2 * lead;
        float pt = 0, pn = 0;
        for (int i = 0; i <= coils * 2; i++) {
          const float t = lead + span * i / (coils * 2), m = i == 0 || i == coils * 2 ? 0 : (i & 1 ? 11 : -11);
          thick(v, l.X(pt, pn), l.Y(pt, pn), l.X(t, m), l.Y(t, m));
          pt = t; pn = m;
        }
        thick(v, l.X(pt, 0), l.Y(pt, 0), l.X(l.len, 0), l.Y(l.len, 0));
        break;
      }
      case 'Z': {
        const int steps = 80;
        float px = a[0], py = a[1] + a[3] / 2;
        for (int i = 1; i <= steps; i++) {
          const float t = (float)i / steps, cx = a[0] + a[2] * t;
          const float cy = a[1] + a[3] / 2 - a[3] / 2 * std::sin(t * a[4] * 6.28318f);
          thick(v, px, py, cx, cy);
          px = cx; py = cy;
        }
        break;
      }
      case 'g': {
        const float gx = a[0], gy = a[1];
        thick(v, gx, gy, gx, gy + 8);
        for (int i = 0; i < 3; i++) thick(v, gx - 14 + i * 5, gy + 8 + i * 5, gx + 14 - i * 5, gy + 8 + i * 5);
        break;
      }
      case 'r': case 'c': case 'l': case 'b': case 's': case 'd': case 'e':
        part(v, cmd, a[0], a[1], a[2], a[3]);
        break;
      default: break;
    }
    p = end;
  }
}

// ---------------------------------------------------------------- saving --

std::string joinIds(const std::vector<std::string> &v) {
  std::string s;
  for (const auto &id : v) s += (s.empty() ? "" : ",") + id;
  return s;
}

void splitIds(const String &s, std::vector<std::string> *v) {
  v->clear();
  std::string t;
  for (const char *p = s.c_str();; p++) {
    if (*p && *p != ',') { t += *p; continue; }
    if (f::findFormula(t.c_str()) >= 0) v->push_back(t);
    t.clear();
    if (!*p) break;
  }
}

void load() {
  Preferences p;
  if (!p.begin("formulas", true)) return;
  splitIds(p.getString("favs", ""), &g_favs);
  splitIds(p.getString("recent", ""), &g_recent);
  p.end();
}

void save() {
  g_saveDue = false;
  Preferences p;
  if (!p.begin("formulas", false)) return;
  p.putString("favs", joinIds(g_favs).c_str());
  p.putString("recent", joinIds(g_recent).c_str());
  p.end();
}

bool isFav(const char *id) { return std::find(g_favs.begin(), g_favs.end(), id) != g_favs.end(); }

// ---------------------------------------------------------------- browse --

constexpr int kRailW = 256, kCardH = 88, kListX = kRailW + 12, kListW = kW - kListX - 12;
int listTop() { return kBarH + 8; }
int listBottom() { return g_searchKeys ? kH - 300 : kH - 6; }
Rect searchBox() { return {300, 8, 680, kBarH - 16}; }
Rect backButton() { return {8, 6, 96, kBarH - 12}; }

void refreshList() {
  g_listCount = 0;
  g_sel = 0;
  g_scroll = 0;
  auto add = [&](int i) {
    for (int k = 0; k < g_listCount; k++) if (g_list[k] == i) return;
    if (i >= 0 && g_listCount < 256) g_list[g_listCount++] = i;
  };
  if (!g_query.empty()) {
    g_listCount = f::search(g_query.c_str(), g_list, 256);
  } else if (g_group == kQuick) {
    for (const auto &id : g_favs) add(f::findFormula(id.c_str()));
    for (const auto &id : g_recent) add(f::findFormula(id.c_str()));
    int q[32];
    const int n = f::quickList(q, 32);
    for (int i = 0; i < n; i++) add(q[i]);
  } else {
    g_listCount = f::inGroup(g_group, g_list, 256);
  }
  g_dirty = true;
}

int railRowH() { return (kH - kBarH - 8) / (f::groupCount() + 1); }

void drawRail() {
  const int h = railRowH();
  for (int i = -1; i < f::groupCount(); i++) {
    const int y = kBarH + 4 + (i + 1) * h;
    const bool on = g_query.empty() && g_group == i;
    if (on) g_fb.fillRoundRect(6, y + 1, kRailW - 12, h - 2, 8, kPanel2);
    const uint32_t c = i < 0 ? kStar : f::group(i)->color;
    g_fb.fillCircle(24, y + h / 2, 7, c);
    text(i < 0 ? "Quick" : f::group(i)->name, 42, y + h / 2, 18, on ? kWhite : 0xC4CCD4);
  }
}

void drawCard(int row, int y) {
  const f::Formula *fm = f::formula(g_list[row]);
  const uint32_t c = f::group(fm->group)->color;
  const int h = kCardH - 8;
  g_fb.fillRoundRect(kListX, y, kListW, h, 12, row == g_sel ? kPanel2 : kPanel);
  g_fb.fillRoundRect(kListX, y, 8, h, 4, c);
  if (row == g_sel) g_fb.drawRoundRect(kListX, y, kListW, h, 12, c);
  const int eqW = 340, textW = kListW - eqW - 40;
  std::string name = fit(fm->name, 24, textW - (isFav(fm->id) ? 28 : 0));
  text(name.c_str(), kListX + 24, y + 26, 24, kWhite);
  if (isFav(fm->id)) g_fb.fillCircle(kListX + 34 + textWidth(name.c_str(), 24), y + 26, 7, kStar);
  text(fit(fm->blurb, 18, textW).c_str(), kListX + 24, y + 58, 18, kDim);
  const f::Node *l, *r;
  const f::Formula *keep = g_f;
  g_f = fm;  // labels for the typesetter
  if (f::equation(fm, 0, &l, &r)) equationAt(l, r, kListX + kListW - eqW - 16, y + 4, eqW, h - 8, 0xDCE3EA, false, 28);
  g_f = keep;
}

void drawBrowse() {
  auto &g = g_fb;
  g.fillRect(0, 0, kW, kBarH, kBarBg);
  const Rect b = backButton();
  g.fillRoundRect(b.x, b.y, b.w, b.h, 10, g_theme->key);
  text("Home", b.x + b.w / 2, b.y + b.h / 2, 24, kWhite, textdatum_t::middle_center);
  text("Formulas", 124, kBarH / 2, 24, kWhite);
  const Rect s = searchBox();
  g.fillRoundRect(s.x, s.y, s.w, s.h, 10, kPanel2);
  if (g_query.empty()) text("Type to search: ohm, watts, kinetic, lens...", s.x + 16, s.y + s.h / 2, 18, kFaint);
  else text((g_query + "_").c_str(), s.x + 16, s.y + s.h / 2, 24, kWhite);
  char count[24];
  snprintf(count, sizeof count, "%d", g_listCount);
  text(count, kW - 24, kBarH / 2, 18, kDim, textdatum_t::middle_right);

  drawRail();
  const int top = listTop(), bottom = listBottom();
  g.setClipRect(kListX, top, kListW, bottom - top);
  for (int i = 0; i < g_listCount; i++) {
    const int y = top + i * kCardH - g_scroll;
    if (y + kCardH < top || y > bottom) continue;
    drawCard(i, y);
  }
  if (!g_listCount) text("Nothing found", kListX + 20, top + 40, 24, kDim);
  g.clearClipRect();
}

int maxScroll() { return std::max(0, g_listCount * kCardH - (listBottom() - listTop())); }

void selectRow(int i) {
  g_sel = std::max(0, std::min(g_listCount - 1, i));
  const int y = g_sel * kCardH, view = listBottom() - listTop();
  if (y < g_scroll) g_scroll = y;
  if (y + kCardH > g_scroll + view) g_scroll = y + kCardH - view;
  g_dirty = true;
}

// ----------------------------------------------------------------- sheet --

constexpr int kLeftX = 16, kLeftW = 476, kDiagY = 74, kDiagH = 286, kEqY = 368, kEqH = 150;
constexpr int kRowsX = 504, kRowsW = kW - kRowsX - 16, kRowsY = 74, kRowsH = 444;
constexpr int kBottomY = 528, kBottomH = kH - kBottomY - 8;

int rowH() { return std::min(74, kRowsH / std::max<int>(1, g_f->varCount)); }
Rect rowRect(int i) { return {kRowsX, kRowsY + i * rowH(), kRowsW, rowH() - 6}; }
Rect fieldRect(int i) { const Rect r = rowRect(i); return {r.x + 348, r.y + 4, 240, r.h - 8}; }
Rect unitRect(int i) { const Rect r = rowRect(i); return {r.x + 594, r.y + 4, 114, r.h - 8}; }
Rect solveRect(int i) { const Rect r = rowRect(i); return {r.x + 714, r.y + 4, r.w - 714 - 4, r.h - 8}; }
Rect starRect() { return {kW - 104 - 64, 6, 56, kBarH - 12}; }
Rect clearRect() { return {kW - 104, 6, 96, kBarH - 12}; }
Rect eqRect() { return {kLeftX, kEqY, kLeftW, kEqH}; }

bool hasUnits(int i) { const f::DimInfo *d = f::dim(g_f->vars[i].dim); return d && d->count > 0; }

int startUnit(const f::Var &v) {
  const f::DimInfo *d = f::dim(v.dim);
  if (!d || !d->count) return 0;
  if (v.unit)
    for (int i = 0; i < d->count; i++)
      if (!strcmp(d->units[i].symbol, v.unit)) return i;
  return d->si;
}

void openFormula(int index) {
  g_fIndex = index;
  g_f = f::formula(index);
  g_sheet.open(g_f);
  for (int i = 0; i < g_f->varCount; i++) {
    g_unit[i] = startUnit(g_f->vars[i]);
    g_unitChosen[i] = false;
  }
  g_focus = 0;
  while (g_focus < g_f->varCount - 1 && g_sheet.hasValue(g_focus)) g_focus++;
  g_editing = false;
  g_edit.clear();
  g_message.clear();
  g_equation = 0;
  g_picker = -1;
  g_screen = Screen::Sheet;
  g_searchKeys = false;
  g_keypad.setRect({});
  auto &r = g_recent;
  r.erase(std::remove(r.begin(), r.end(), std::string(g_f->id)), r.end());
  r.insert(r.begin(), g_f->id);
  if (r.size() > 8) r.resize(8);
  g_saveDue = true;
  g_dirty = true;
}

void closeFormula() {
  g_screen = Screen::Browse;
  g_f = nullptr;
  g_editing = false;
  g_queryOld = true;
  if (g_group == kQuick && g_query.empty()) refreshList();
  g_dirty = true;
}

// Solved values read best in a scaled unit (3.3 mA), unless one was chosen.
int shownUnit(int i) {
  if (!hasUnits(i)) return 0;
  if (g_unitChosen[i] || g_sheet.cell(i) != f::Cell::Solved) return g_unit[i];
  return f::bestUnit(g_sheet.value(i), g_f->vars[i].dim, g_unit[i]);
}

std::string unitSymbol(int i, int unit) {
  const f::DimInfo *d = f::dim(g_f->vars[i].dim);
  return d && unit < d->count ? pretty(d->units[unit].symbol) : "";
}

void commit() {
  if (!g_editing) return;
  g_editing = false;
  if (g_edit.empty()) return;
  double si = 0;
  int used = -1;
  const f::Var &v = g_f->vars[g_focus];
  if (!f::parseValue(g_edit.c_str(), v.dim, g_unit[g_focus], &si, &used)) {
    g_message = "Can't read \"" + g_edit + "\"";
    g_editing = true;
    return;
  }
  if (used >= 0) {
    g_unit[g_focus] = used;
    g_unitChosen[g_focus] = true;
  }
  g_message.clear();
  g_sheet.set(g_focus, si);
  g_edit.clear();
}

void focusNext(int dir) {
  commit();
  if (g_editing) return;  // a value that can't be read stays to be fixed
  const int n = g_f->varCount;
  // Forward, the next row without a value is usually the next one wanted.
  if (dir > 0)
    for (int k = 1; k < n; k++) {
      const int i = (g_focus + k) % n;
      if (g_sheet.cell(i) == f::Cell::Empty) { g_focus = i; return; }
    }
  g_focus = (g_focus + dir + n) % n;
}

void drawRow(int i) {
  auto &g = g_fb;
  const f::Var &v = g_f->vars[i];
  const Rect r = rowRect(i);
  const uint32_t accent = f::group(g_f->group)->color;
  const bool focus = i == g_focus;
  const f::Cell cell = g_sheet.cell(i);
  g.fillRoundRect(r.x, r.y, r.w, r.h, 10, focus ? kPanel2 : kPanel);
  if (focus) g.drawRoundRect(r.x, r.y, r.w, r.h, 10, accent);

  const int sz = r.h >= 56 ? 28 : 24, lw = label(v.label, 0, 0, sz, kWhite, false);
  label(v.label, r.x + 46 - lw / 2, r.y + r.h / 2 + sz * 3 / 8, sz, cell == f::Cell::Solved ? kAnswer : kWhite);
  const int infoX = r.x + 96, infoW = 248;
  if (r.h >= 52) {
    text(fit(v.name, 18, infoW).c_str(), infoX, r.y + r.h / 2 - 12, 18, kWhite);
    // What the field is measured in, in words.
    std::string kind = "A plain number, no unit";
    if (hasUnits(i)) {
      const u::Unit &si = f::dim(v.dim)->units[f::dim(v.dim)->si];
      kind = "In " + std::string(si.name) + " (" + pretty(si.symbol) + ")";
    }
    text(fit(kind.c_str(), 16, infoW).c_str(), infoX, r.y + r.h / 2 + 13, 16, kDim);
  } else {
    text(fit(v.name, 18, infoW).c_str(), infoX, r.y + r.h / 2, 18, kWhite);
  }

  const Rect fr = fieldRect(i);
  g.fillRoundRect(fr.x, fr.y, fr.w, fr.h, 8, focus && g_editing ? 0x10161C : kChip);
  if (focus && g_editing) {
    text(fit((g_edit + "_").c_str(), 24, fr.w - 20).c_str(), fr.x + fr.w - 10, fr.y + fr.h / 2, 24, kWhite, textdatum_t::middle_right);
  } else if (g_sheet.hasValue(i)) {
    const std::string s = f::formatValue(g_sheet.value(i), v.dim, shownUnit(i));
    const uint32_t c = cell == f::Cell::Solved ? kAnswer : cell == f::Cell::Preset ? kAccent : kWhite;
    text(fit(s.c_str(), 24, fr.w - 20).c_str(), fr.x + fr.w - 10, fr.y + fr.h / 2, 24, c, textdatum_t::middle_right);
  }

  if (hasUnits(i)) {
    const Rect ur = unitRect(i);
    g.fillRoundRect(ur.x, ur.y, ur.w, ur.h, 8, g_theme->key);
    const std::string s = unitSymbol(i, shownUnit(i));
    text(fit(s.c_str(), 20, ur.w - 8).c_str(), ur.x + ur.w / 2, ur.y + ur.h / 2, 20, kWhite, textdatum_t::middle_center);
  }
  const Rect sr = solveRect(i);
  g.fillRoundRect(sr.x, sr.y, sr.w, sr.h, 8, cell == f::Cell::Solved ? 0x2B4A33 : g_theme->key);
  text("=", sr.x + sr.w / 2, sr.y + sr.h / 2, 24, cell == f::Cell::Solved ? kAnswer : kDim, textdatum_t::middle_center);
}

void drawEquationCard() {
  auto &g = g_fb;
  const Rect c = eqRect();
  g.fillRoundRect(c.x, c.y, c.w, c.h, 12, kPanel);
  const bool how = g_focus >= 0 && g_sheet.cell(g_focus) == f::Cell::Solved && g_sheet.howLhs(g_focus);
  const int footer = 30;
  if (how) {
    equationAt(g_sheet.howLhs(g_focus), g_sheet.howRhs(g_focus), c.x + 10, c.y + 6, c.w - 20, c.h - footer - 8, kAnswer, true);
  } else {
    const f::Node *l, *r;
    if (f::equation(g_f, g_equation, &l, &r)) equationAt(l, r, c.x + 10, c.y + 6, c.w - 20, c.h - footer - 8, kWhite, true);
  }

  // The state of the sheet, on the card's last line.
  std::string s;
  uint32_t col = kDim;
  const int missing = g_sheet.missing();
  if (!g_message.empty()) {
    s = g_message;
    col = kErr;
  } else if (g_sheet.altVar() >= 0) {
    // Two solutions: error() only says which is shown, so this says more.
    const int a = g_sheet.altVar();
    s = "Two solutions. The other: " + f::formatValue(g_sheet.altValue(), g_f->vars[a].dim, shownUnit(a)) + " " +
        unitSymbol(a, shownUnit(a));
    col = kWarm;
  } else if (*g_sheet.error()) {
    s = g_sheet.error();
    col = kErr;
  } else if (missing > 0) {
    char b[48];
    snprintf(b, sizeof b, "Enter %d more value%s", missing, missing == 1 ? "" : "s");
    s = b;
  } else if (!how && g_f->equationCount > 1) {
    char b[48];
    snprintf(b, sizeof b, "Equation %d of %d: tap for the next", g_equation + 1, g_f->equationCount);
    s = b;
  } else {
    s = "Solved";
    col = kAnswer;
  }
  text(fit(s.c_str(), 18, c.w - 28).c_str(), c.x + c.w / 2, c.y + c.h - 18, 18, col, textdatum_t::middle_center);
}

// The bottom strip: help on the focused variable, and its value in common units.
constexpr int kHelpW = 470, kChipCols = 4, kChipH = 52;
int g_chipUnit[16];
int g_chipCount = 0;
Rect chipRect(int k) {
  const int x0 = kLeftX + kHelpW + 16, w = (kW - x0 - 16 - (kChipCols - 1) * 8) / kChipCols;
  return {x0 + (k % kChipCols) * (w + 8), kBottomY + 26 + (k / kChipCols) * (kChipH + 6), w, kChipH};
}

// A sense of scale for each kind of quantity, under the variable's own help.
const char *scaleHint(int d) {
  switch (d) {
    case u::kVoltage: return "A AA cell is 1.5 V, a car 12 V, US mains 120 V.";
    case u::kCurrent: return "An LED takes about 20 mA, a toaster about 8 A.";
    case u::kResistance: return "Parts run from ohms to megohms: 4.7k is 4700 ohms.";
    case u::kCapacitance: return "Parts run from pF through nF to thousands of uF.";
    case f::kInductance: return "Parts run from uH to a few H.";
    case u::kPower: return "An LED is 0.06 W, a kettle 1500 W, a horsepower 746 W.";
    case u::kEnergy: return "A AA cell holds about 10 kJ; 1 kWh is 3.6 MJ.";
    case u::kFrequency: return "Mains is 50 or 60 Hz, FM radio about 100 MHz.";
    case u::kLength: return "Type mixed units too: 5 ft 3 in.";
    case u::kMass: return "A litre of water is 1 kg.";
    case u::kForce: return "1 kg weighs about 9.8 N on Earth.";
    case u::kPressure: return "The atmosphere is 101 kPa, about 14.7 psi.";
    case u::kTemperature: return "Absolute: water freezes at 0 C, or 273.15 K.";
    case f::kTempDiff: return "A difference: 10 C deg warmer is 10 K, 18 F deg.";
    case u::kSpeed: return "Walking is about 1.4 m/s; 60 mph is 26.8 m/s.";
    case u::kAngle: return "A full turn is 360 deg, or 6.28 radians.";
    case f::kAngularSpeed: return "1000 rpm is about 105 rad/s.";
    case u::kDensity: return "Water is 1000 kg/m^3, air about 1.2.";
    case u::kTime: return "Type mixed units too: 1h 20m.";
    case f::kCharge: return "A AA cell holds about 2500 mAh.";
    default: return "";
  }
}

void drawBottom() {
  auto &g = g_fb;
  const f::Var &v = g_f->vars[g_focus];
  g.fillRoundRect(kLeftX, kBottomY, kW - 2 * kLeftX, kBottomH, 12, kPanel);
  const int lw = label(v.label, kLeftX + 16, kBottomY + 32, 24, kWhite);
  text(fit(v.name, 24, kHelpW - lw - 40).c_str(), kLeftX + 28 + lw, kBottomY + 24, 24, kWhite);
  int y = kBottomY + 46;
  y += wrap(v.help, kLeftX + 16, y, kHelpW - 20, 18, 0xB8C2CC, 3);
  std::string more = "A plain number, no unit.";
  if (hasUnits(g_focus)) {
    const u::Unit &si = f::dim(v.dim)->units[f::dim(v.dim)->si];
    more = "Measured in " + std::string(si.name) + " (" + pretty(si.symbol) + ").";
    if (strlen(v.help) < 40) more += std::string(" ") + scaleHint(v.dim);  // longer help says it already
  }
  wrap(more.c_str(), kLeftX + 16, y + 4, kHelpW - 20, 18, kDim, std::max(1, (kBottomY + kBottomH - y - 8) / 25));

  g_chipCount = 0;
  if (!hasUnits(g_focus)) {
    wrap(g_f->blurb, kLeftX + kHelpW + 16, kBottomY + 14, kW - kLeftX * 2 - kHelpW - 40, 18, kDim, 6);
    return;
  }
  const f::DimInfo *d = f::dim(v.dim);
  const bool have = g_sheet.hasValue(g_focus);
  text(have ? "THE SAME VALUE IN OTHER UNITS: TAP TO USE" : "COMMON UNITS: TAP TO USE", kLeftX + kHelpW + 16, kBottomY + 13, 12, kDim);
  int commons = 0;  // few common units: show the rest too
  for (int k = 0; k < d->count; k++) commons += (d->units[k].flags & u::kCommon) != 0;
  const int current = shownUnit(g_focus), maxChips = 2 * kChipCols + (kBottomH > 170 ? 4 : 0);
  for (int k = 0; k < d->count && g_chipCount < std::min(12, maxChips); k++) {
    if (!(d->units[k].flags & u::kCommon) && k != current && commons >= 4) continue;
    g_chipUnit[g_chipCount++] = k;
  }
  for (int k = 0; k < g_chipCount; k++) {
    const Rect c = chipRect(k);
    if (c.y + c.h > kBottomY + kBottomH) { g_chipCount = k; break; }
    const int un = g_chipUnit[k];
    const bool on = un == current;
    g.fillRoundRect(c.x, c.y, c.w, c.h, 8, on ? g_theme->keyLit : kChip);
    std::string s = have ? f::formatValue(g_sheet.value(g_focus), v.dim, un) + " " : "";
    s += pretty(d->units[un].symbol);
    const int sz = textWidth(s.c_str(), 24) <= c.w - 12 ? 24 : textWidth(s.c_str(), 20) <= c.w - 12 ? 20 : 16;
    text(fit(s.c_str(), sz, c.w - 12).c_str(), c.x + c.w / 2, c.y + c.h / 2, sz, on ? kWhite : 0xD8DEE4, textdatum_t::middle_center);
  }
}

// The on-screen number keys, in the bottom strip while typing without the keyboard.
const char *const kPadKeys[2][11] = {{"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "."},
                                     {"k", "M", "m", "μ", "n", "p", "E", "-", "/", "Del", "Next"}};
Rect padKey(int row, int col) {
  const int w = (kW - 2 * kLeftX - 10 * 6) / 11, h = (kBottomH - 6) / 2;
  return {kLeftX + col * (w + 6), kBottomY + row * (h + 6), w, h};
}

bool padShown() { return !g_kbPresent && g_editing; }

void drawPad() {
  for (int r = 0; r < 2; r++)
    for (int c = 0; c < 11; c++) {
      const Rect k = padKey(r, c);
      g_fb.fillRoundRect(k.x, k.y, k.w, k.h, 10, r == 0 ? kDigitKey : kFnKey);
      text(kPadKeys[r][c], k.x + k.w / 2, k.y + k.h / 2, 24, kWhite, textdatum_t::middle_center);
    }
}

// The unit picker: every unit of the kind, then handy values, over the rows.
struct Handy {
  std::string name;
  double value;
};
std::vector<Handy> g_handy;
constexpr int kPickX = 60, kPickY = 84, kPickW = kW - 120, kPickH = kH - 104, kPickCols = 5, kPickRowH = 56;
Rect pickRect(int k) {
  const int w = (kPickW - 32 - (kPickCols - 1) * 8) / kPickCols;
  return {kPickX + 16 + (k % kPickCols) * (w + 8), kPickY + 54 + (k / kPickCols) * (kPickRowH + 6), w, kPickRowH};
}

void loadHandy(const char *s) {
  g_handy.clear();
  if (!s) return;
  std::string t;
  for (const char *p = s;; p++) {
    if (*p && *p != '|') { t += *p; continue; }
    const size_t e = t.find('=');
    if (e != std::string::npos) g_handy.push_back({t.substr(0, e), atof(t.c_str() + e + 1)});
    t.clear();
    if (!*p) break;
  }
}

void drawPicker() {
  auto &g = g_fb;
  const f::Var &v = g_f->vars[g_picker];
  const f::DimInfo *d = f::dim(v.dim);
  g.fillRoundRect(kPickX, kPickY, kPickW, kPickH, 14, kPanel2);
  g.drawRoundRect(kPickX, kPickY, kPickW, kPickH, 14, kFaint);
  text((std::string(v.name) + ": units").c_str(), kPickX + 20, kPickY + 28, 24, kWhite);
  text("Tap outside to close", kPickX + kPickW - 20, kPickY + 28, 18, kDim, textdatum_t::middle_right);
  const int current = shownUnit(g_picker);
  const int n = d ? d->count : 0;
  for (int k = 0; k < n; k++) {
    const Rect c = pickRect(k);
    g.fillRoundRect(c.x, c.y, c.w, c.h, 8, k == current ? g_theme->keyLit : kChip);
    std::string s = pretty(d->units[k].symbol);
    if (textWidth((s + "  " + d->units[k].name).c_str(), 18) <= c.w - 12) s += std::string("  ") + d->units[k].name;
    text(fit(s.c_str(), 18, c.w - 12).c_str(), c.x + c.w / 2, c.y + c.h / 2, 18, kWhite, textdatum_t::middle_center);
  }
  if (g_handy.empty()) return;
  const int first = (n + kPickCols - 1) / kPickCols * kPickCols;
  const int y = pickRect(first).y;
  text("HANDY VALUES", kPickX + 20, y + 6, 12, kDim);
  for (size_t k = 0; k < g_handy.size(); k++) {
    Rect c = pickRect(first + (int)k);
    c.y += 20;
    if (c.y + c.h > kPickY + kPickH - 8) break;
    g.fillRoundRect(c.x, c.y, c.w, c.h, 8, 0x26323E);
    text(fit(g_handy[k].name.c_str(), 18, c.w - 12).c_str(), c.x + c.w / 2, c.y + c.h / 2, 18, kAccent, textdatum_t::middle_center);
  }
}

void drawSheet() {
  auto &g = g_fb;
  const uint32_t accent = f::group(g_f->group)->color;
  g.fillRect(0, 0, kW, kBarH, kBarBg);
  const Rect b = backButton();
  g.fillRoundRect(b.x, b.y, b.w, b.h, 10, g_theme->key);
  text("Back", b.x + b.w / 2, b.y + b.h / 2, 24, kWhite, textdatum_t::middle_center);
  g.fillCircle(124, kBarH / 2, 8, accent);
  text(fit(g_f->name, 24, 880).c_str(), 142, kBarH / 2, 24, kWhite);
  const Rect st = starRect(), cl = clearRect();
  const bool fav = isFav(g_f->id);
  g.fillRoundRect(st.x, st.y, st.w, st.h, 10, fav ? 0x5A4A1C : g_theme->key);
  g.fillCircle(st.x + st.w / 2, st.y + st.h / 2, 9, fav ? kStar : kFaint);
  g.fillRoundRect(cl.x, cl.y, cl.w, cl.h, 10, g_theme->key);
  text("Clear", cl.x + cl.w / 2, cl.y + cl.h / 2, 24, kWhite, textdatum_t::middle_center);

  g.fillRoundRect(kLeftX, kDiagY, kLeftW, kDiagH, 12, kPanel);
  if (g_f->diagram) drawDiagram(g_f->diagram, kLeftX + 6, kDiagY + 6, kLeftW - 12, kDiagH - 12, accent);
  drawEquationCard();
  for (int i = 0; i < g_f->varCount; i++) drawRow(i);
  if (padShown()) drawPad();
  else drawBottom();
  if (g_picker >= 0) drawPicker();
}

void render() {
  g_dirty = false;
  if (!g_fbReady) {
    g_fb.setPsram(true);
    g_fb.setColorDepth(16);
    g_fb.createSprite(kW, kH);
    g_fbReady = true;
  }
  g_fb.fillSprite(g_theme->background);
  if (g_screen == Screen::Browse) drawBrowse();
  else drawSheet();
  g_fb.pushSprite(0, 0);
  if (g_searchKeys) {
    g_keypadDraw.invalidate();
    g_keypadDraw.draw(M5.Display, g_keypad, *g_theme);
  }
}

// ----------------------------------------------------------------- input --

void openPicker(int i) {
  commit();
  g_focus = i;
  g_picker = i;
  loadHandy(g_f->vars[i].presets);
  g_dirty = true;
}

void pickerTap(int x, int y) {
  const int i = g_picker;
  const f::DimInfo *d = f::dim(g_f->vars[i].dim);
  const int n = d ? d->count : 0;
  g_picker = -1;
  g_dirty = true;
  for (int k = 0; k < n; k++)
    if (pickRect(k).contains(x, y)) {
      g_unit[i] = k;
      g_unitChosen[i] = true;
      return;
    }
  const int first = (n + kPickCols - 1) / kPickCols * kPickCols;
  for (size_t k = 0; k < g_handy.size(); k++) {
    Rect c = pickRect(first + (int)k);
    c.y += 20;
    if (c.contains(x, y)) {
      g_sheet.set(i, g_handy[k].value);
      return;
    }
  }
}

void beginEdit() {
  if (g_editing) return;
  g_editing = true;
  g_edit.clear();
  g_message.clear();
}

void padTap(int x, int y) {
  for (int r = 0; r < 2; r++)
    for (int c = 0; c < 11; c++) {
      if (!padKey(r, c).contains(x, y)) continue;
      const char *k = kPadKeys[r][c];
      if (!strcmp(k, "Del")) {
        if (!g_edit.empty()) g_edit.pop_back();
      } else if (!strcmp(k, "Next")) {
        const bool empty = g_edit.empty();
        focusNext(1);
        if (!g_editing && !empty && g_sheet.cell(g_focus) == f::Cell::Empty) beginEdit();
      } else if (!strcmp(k, "μ")) {
        g_edit += 'u';
      } else if (!strcmp(k, "E")) {
        g_edit += 'e';
      } else {
        g_edit += k;
      }
      g_dirty = true;
      return;
    }
}

void sheetTap(int x, int y) {
  g_dirty = true;
  if (g_picker >= 0) { pickerTap(x, y); return; }
  if (backButton().contains(x, y)) { commit(); closeFormula(); return; }
  if (starRect().contains(x, y)) {
    auto it = std::find(g_favs.begin(), g_favs.end(), g_f->id);
    if (it != g_favs.end()) g_favs.erase(it);
    else g_favs.insert(g_favs.begin(), g_f->id);
    g_saveDue = true;
    return;
  }
  if (clearRect().contains(x, y)) {
    g_editing = false;
    g_edit.clear();
    g_message.clear();
    g_sheet.clearAll();
    return;
  }
  if (padShown() && y >= kBottomY) { padTap(x, y); return; }
  for (int i = 0; i < g_f->varCount; i++) {
    if (!rowRect(i).contains(x, y)) continue;
    if (solveRect(i).contains(x, y)) {
      commit();
      if (g_editing) return;
      g_focus = i;
      if (g_sheet.cell(i) == f::Cell::Entered || g_sheet.cell(i) == f::Cell::Preset) {
        // Swap roles: a solved row keeps its value as an input in i's place, so
        // the sheet stays solved.
        int keep = -1;
        for (int k = 0; k < g_f->varCount && keep < 0; k++)
          if (g_sheet.cell(k) == f::Cell::Solved) keep = k;
        const double v = keep >= 0 ? g_sheet.value(keep) : 0;
        g_sheet.solveFor(i);
        if (keep >= 0) g_sheet.set(keep, v);
      }
      return;
    }
    if (unitRect(i).contains(x, y) && hasUnits(i)) { openPicker(i); return; }
    if (i != g_focus) {
      commit();
      if (g_editing) return;
      g_focus = i;
    }
    if (fieldRect(i).contains(x, y) && !g_kbPresent) beginEdit();
    return;
  }
  if (eqRect().contains(x, y)) {
    g_equation = (g_equation + 1) % std::max<int>(1, g_f->equationCount);
    return;
  }
  if (g_editing) commit();
  for (int k = 0; k < g_chipCount; k++)
    if (chipRect(k).contains(x, y)) {
      g_unit[g_focus] = g_chipUnit[k];
      g_unitChosen[g_focus] = true;
      return;
    }
}

void browseTap(int x, int y) {
  g_dirty = true;
  if (backButton().contains(x, y)) { appGoHome(); return; }
  if (searchBox().contains(x, y)) {
    if (!g_kbPresent) {
      g_searchKeys = !g_searchKeys;
      g_keypad.setRect(g_searchKeys ? Rect{0, kH - 300, kW, 300} : Rect{});
    }
    return;
  }
  if (x < kRailW) {
    const int i = (y - kBarH - 4) / railRowH() - 1;
    if (y < kBarH + 4 || i >= f::groupCount()) return;
    g_group = i;
    g_query.clear();
    refreshList();
    return;
  }
  if (y >= listTop() && y < listBottom()) {
    const int row = (y - listTop() + g_scroll) / kCardH;
    if (row >= 0 && row < g_listCount) openFormula(g_list[row]);
  }
}

void tapAt(int x, int y) {
  if (g_screen == Screen::Browse) browseTap(x, y);
  else sheetTap(x, y);
}

void typed(const KeyText &t) {
  g_dirty = true;
  if (g_screen == Screen::Browse) {
    if (t.special == key::Escape) {
      if (!g_query.empty()) { g_query.clear(); refreshList(); }
      else appGoHome();
    } else if (t.special == key::Enter || t.special == key::KpEnter) {
      if (g_listCount) openFormula(g_list[g_sel]);
    } else if (t.special == key::Backspace) {
      if (!g_query.empty()) { g_query.pop_back(); refreshList(); }
    } else if (t.special == key::Down) {
      selectRow(g_sel + 1);
    } else if (t.special == key::Up) {
      selectRow(g_sel - 1);
    } else if (t.ch >= ' ' && t.ch <= '~' && !t.ctrl && g_query.size() < 40) {
      if (g_queryOld) g_query.clear();
      g_queryOld = false;
      g_query += t.ch;
      refreshList();
    }
    return;
  }
  if (g_picker >= 0) {
    if (t.special == key::Escape) g_picker = -1;
    return;
  }
  if (t.special == key::Escape) {
    if (g_editing) { g_editing = false; g_edit.clear(); g_message.clear(); }
    else closeFormula();
  } else if (t.special == key::Enter || t.special == key::KpEnter || t.special == key::Tab || t.special == key::Down) {
    focusNext(1);
  } else if (t.special == key::Up) {
    focusNext(-1);
  } else if (t.special == key::Backspace) {
    if (g_editing) {
      if (!g_edit.empty()) g_edit.pop_back();
      if (g_edit.empty()) g_editing = false;
    } else {
      g_sheet.clear(g_focus);
    }
  } else if (t.ch >= ' ' && t.ch <= '~' && !t.ctrl) {
    if (!g_editing && t.ch == ' ') return;
    beginEdit();
    if (g_edit.size() < 40) g_edit += t.ch;
  }
}

void handleTouch() {
  if (!M5.Touch.getCount()) return;
  const auto &t = M5.Touch.getDetail(0);
  if (t.wasPressed()) {
    g_down = true;
    g_dragging = false;
    g_downX = t.x;
    g_downY = t.y;
    g_downScroll = g_scroll;
  }
  if (!g_down) return;
  const bool canDrag = g_screen == Screen::Browse && g_downX >= kListX && g_downY >= listTop() && g_downY < listBottom();
  if (t.isPressed() && canDrag) {
    if (std::abs(t.y - g_downY) > 14) g_dragging = true;
    if (g_dragging) {
      const int s = std::max(0, std::min(maxScroll(), g_downScroll + g_downY - t.y));
      if (s != g_scroll) {
        g_scroll = s;
        g_dirty = true;
      }
    }
  }
  if (t.wasReleased()) {
    g_down = false;
    if (!g_dragging) tapAt(g_downX, g_downY);
  }
}

}  // namespace

void formulasBegin(Tab5Keyboard &kb, const Theme &theme) {
  g_kb = &kb;
  g_theme = &theme;
  static bool loaded = false;
  if (!loaded) {
    loaded = true;
    load();
    g_keypad.setPages(kPages, 1);
  }
  g_kbPresent = kb.present();
  g_screen = Screen::Browse;
  g_f = nullptr;
  g_query.clear();
  g_searchKeys = false;
  g_keypad.setRect({});
  refreshList();
}

void formulasLoop(uint32_t) {
  if (g_kb->present() != g_kbPresent) {
    g_kbPresent = !g_kbPresent;
    if (g_kbPresent && g_searchKeys) {
      g_searchKeys = false;
      g_keypad.setRect({});
    }
    g_dirty = true;
  }
  if (g_searchKeys) {
    TouchPoint touches[kMaxTouches];
    const int n = readM5Touch(touches);
    g_keypad.handle(touches, n);
    g_keypadDraw.draw(M5.Display, g_keypad, *g_theme);
  }
  if (!(g_searchKeys && M5.Touch.getCount() && M5.Touch.getDetail(0).y >= kH - 300 && !g_down)) handleTouch();
  KeyEvent e;
  KeyText t;
  while (g_kb->next(&e))
    if (KeyText::from(e, &t)) typed(t);
  while (g_keypad.next(&e))
    if (KeyText::from(e, &t)) typed(t);
  if (g_dirty) render();
  if (g_saveDue) save();
}

void formulasEnd() {
  if (g_saveDue) save();
  if (g_fbReady) {
    g_fb.deleteSprite();  // 1.8 MB of PSRAM, back for the other apps
    g_fbReady = false;
  }
}

void formulasType(char c) {
  KeyText t;
  if (c == '\n' || c == '\r') t.special = key::Enter;
  else if (c == 27) t.special = key::Escape;
  else if (c == 8 || c == 127) t.special = key::Backspace;
  else if (c == '\t') t.special = key::Tab;
  else t.ch = c;
  typed(t);
  if (g_dirty) render();
}

void formulasTap(int x, int y) {
  tapAt(x, y);
  if (g_dirty) render();
}

void formulasGallery() {
  // Every distinct drawing, twelve to a page.
  static int page = 0;
  std::vector<const f::Formula *> all;
  for (int i = 0; i < f::formulaCount(); i++) {
    const f::Formula *fm = f::formula(i);
    if (!fm->diagram) continue;
    bool seen = false;
    for (const auto *o : all) if (o->diagram == fm->diagram || !strcmp(o->diagram, fm->diagram)) seen = true;
    if (!seen) all.push_back(fm);
  }
  const int pages = ((int)all.size() + 11) / 12;
  if (page >= pages) page = 0;
  if (!g_fbReady) render();
  g_fb.fillSprite(g_theme->background);
  for (int k = 0; k < 12; k++) {
    const int i = page * 12 + k;
    if (i >= (int)all.size()) break;
    const int x = (k % 4) * 320, y = (k / 4) * 240;
    g_fb.fillRect(x + 4, y + 4, 312, 232, kPanel);
    drawDiagram(all[i]->diagram, x + 6, y + 6, 308, 208, f::group(all[i]->group)->color);
    text(fit(all[i]->name, 12, 300).c_str(), x + 10, y + 225, 12, kDim);
  }
  char b[32];
  snprintf(b, sizeof b, "page %d of %d", page + 1, pages);
  text(b, kW - 10, 12, 12, kWhite, textdatum_t::middle_right);
  g_fb.pushSprite(0, 0);
  page++;
}

}  // namespace mt
