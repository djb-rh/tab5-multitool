#include "RenderM5GFX.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace t5m {

void KeypadRenderer::drawKey(lgfx::LovyanGFX &g, const KeyDef &d, const char *label, const Rect &r, bool lit,
                             const Theme &t) {
  if (d.kind == KeyDef::Gap) {
    g.fillRect(r.x, r.y, r.w, r.h, t.background);
    return;
  }
  const bool special = d.kind != KeyDef::Key || (label[0] && label[1]);
  g.fillRoundRect(r.x, r.y, r.w, r.h, t.radius, lit ? t.keyLit : special ? t.keyMod : t.key);
  if (!label[0]) return;
  // Single characters large, words smaller.
  const bool single = label[1] == 0;
  g.setFont(single ? &fonts::DejaVu40 : (r.w < 90 ? &fonts::DejaVu18 : &fonts::DejaVu24));
  g.setTextDatum(textdatum_t::middle_center);
  g.setTextColor(lit ? (uint32_t)0xFFFFFF : special ? t.textDim : t.text);
  g.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
}

void KeypadRenderer::draw(lgfx::LovyanGFX &g, Keypad &kp, const Theme &t) {
  if (kp.page() != page_) full_ = true;
  if (!full_ && !kp.dirty()) return;
  const int n = kp.keyCount();
  g.startWrite();
  if (full_) {
    const Rect &r = kp.rect();
    g.fillRect(r.x, r.y, r.w, r.h, t.background);
  }
  for (int i = 0; i < n && i < 96; i++) {
    KeyDef d;
    Rect r;
    bool lit;
    if (!kp.keyAt(i, &d, &r, &lit)) continue;
    char buf[8];
    const char *label = kp.labelFor(i, buf, sizeof buf);
    if (!full_ && lit == lit_[i] && strncmp(label, label_[i], sizeof label_[i]) == 0) continue;
    lit_[i] = lit;
    strncpy(label_[i], label, sizeof label_[i] - 1);
    drawKey(g, d, label, r, lit, t);
  }
  g.endWrite();
  full_ = false;
  page_ = kp.page();
  kp.clean();
}

void TrackpadRenderer::draw(lgfx::LovyanGFX &g, const Trackpad &tp, const Theme &t) {
  const uint8_t held = tp.stripButtons();
  if (!full_ && held == buttons_) return;
  g.startWrite();
  if (full_) {
    const Rect &r = tp.rect();
    g.fillRect(r.x, r.y, r.w, r.h, t.background);
    Rect pad;
    tp.padRect(&pad);
    g.fillRoundRect(pad.x + 4, pad.y + 4, pad.w - 8, pad.h - 8, t.radius * 2, t.pad);
    g.drawRoundRect(pad.x + 4, pad.y + 4, pad.w - 8, pad.h - 8, t.radius * 2, t.padEdge);
    g.setFont(&fonts::DejaVu18);
    g.setTextDatum(textdatum_t::bottom_center);
    g.setTextColor(t.textDim);
    g.drawString("tap = click   two fingers = right-click / scroll   tap + drag = drag", pad.x + pad.w / 2,
                 pad.y + pad.h - 14);
  }
  static const uint8_t kBit[3] = {button::Left, button::Middle, button::Right};
  for (int i = 0; i < 3; i++) {
    Rect b;
    if (!tp.buttonRect(i, &b)) break;
    const bool lit = held & kBit[i];
    g.fillRoundRect(b.x + 4, b.y + 4, b.w - 8, b.h - 8, t.radius, lit ? t.keyLit : t.key);
  }
  g.endWrite();
  full_ = false;
  buttons_ = held;
}

}  // namespace t5m

namespace t5m {

namespace {
// Colours here are RGB888 and must reach LovyanGFX as uint32_t: a plain int
// literal is taken as RGB565 (0x7CFC9A green comes out pink).
constexpr uint32_t kWhite = 0xFFFFFF, kInk = 0x101418;
// A darker or lighter shade of an RGB888 colour, for pressed buttons.
uint32_t shade(uint32_t c, int pct) {
  auto ch = [&](int shift) {
    int v = (int)((c >> shift) & 0xFF) * pct / 100;
    return (uint32_t)(v > 255 ? 255 : v) << shift;
  };
  return ch(16) | ch(8) | ch(0);
}
// Perceived brightness, to pick black or white text.
bool light(uint32_t c) {
  const int r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
  return r * 299 + g * 587 + b * 114 > 150000;
}
}  // namespace

void MacroPadRenderer::drawButton(lgfx::LovyanGFX &g, const MacroPad &mp, int i, const Theme &t) {
  const MacroPage *page = mp.current();
  Rect r;
  if (!page || !mp.buttonRect(i, &r)) return;
  if (i >= (int)page->buttons.size() || page->buttons[i].steps.empty()) {
    g.drawRoundRect(r.x, r.y, r.w, r.h, t.radius, t.pad);  // an empty slot
    return;
  }
  const MacroButton &b = page->buttons[i];
  const bool down = mp.pressed() == i;
  const uint32_t fill = down ? shade(b.color, 150) : b.color;
  g.fillRoundRect(r.x, r.y, r.w, r.h, t.radius, fill);
  if (down) g.drawRoundRect(r.x, r.y, r.w, r.h, t.radius, kWhite);
  // Up to three lines, split at "\n", as large as the button allows.
  std::string lines[3];
  int nl = 0;
  size_t start = 0;
  while (nl < 3) {
    const size_t e = b.label.find('\n', start);
    lines[nl++] = b.label.substr(start, e == std::string::npos ? std::string::npos : e - start);
    if (e == std::string::npos) break;
    start = e + 1;
  }
  const lgfx::GFXfont *font = &fonts::DejaVu40;
  g.setFont(font);
  int widest = 0;
  for (int k = 0; k < nl; k++) widest = std::max(widest, (int)g.textWidth(lines[k].c_str()));
  if (widest > r.w - 16 || nl * g.fontHeight() > r.h - 12) font = &fonts::DejaVu24;
  g.setFont(font);
  widest = 0;
  for (int k = 0; k < nl; k++) widest = std::max(widest, (int)g.textWidth(lines[k].c_str()));
  if (widest > r.w - 12) g.setFont(font = &fonts::DejaVu18);
  g.setTextDatum(textdatum_t::middle_center);
  g.setTextColor(light(fill) ? kInk : kWhite);
  const int lh = g.fontHeight() + 4;
  const int y0 = r.y + r.h / 2 - (nl - 1) * lh / 2;
  for (int k = 0; k < nl; k++) g.drawString(lines[k].c_str(), r.x + r.w / 2, y0 + k * lh);
}

void MacroPadRenderer::drawTab(lgfx::LovyanGFX &g, const MacroPad &mp, int i, const Theme &t) {
  Rect r;
  if (!mp.tabRect(i, &r)) return;
  const bool on = i == mp.page();
  const uint32_t fill = on ? t.keyLit : t.key;
  g.fillRoundRect(r.x, r.y, r.w, r.h, t.radius, mp.pressedTab() == i ? shade(fill, 150) : fill);
  g.setFont(&fonts::DejaVu24);
  g.setTextDatum(textdatum_t::middle_center);
  g.setTextColor(on ? kWhite : t.text);
  g.drawString(mp.pageName(i), r.x + r.w / 2, r.y + r.h / 2);
}

void MacroPadRenderer::draw(lgfx::LovyanGFX &g, MacroPad &mp, const Theme &t) {
  if (mp.page() != page_) full_ = true;
  if (!full_ && !mp.dirty()) return;
  g.startWrite();
  if (full_) {
    const Rect &area = mp.rect();
    g.fillRect(area.x, area.y, area.w, area.h, t.background);
    const MacroPage *page = mp.current();
    if (!page) {
      g.setFont(&fonts::DejaVu24);
      g.setTextDatum(textdatum_t::middle_center);
      g.setTextColor(t.textDim);
      g.drawString("No macros yet", area.x + area.w / 2, area.y + area.h / 2);
    } else {
      for (int i = 0; i < page->cols * page->rows; i++) drawButton(g, mp, i, t);
      for (int i = 0; i < mp.pageCount(); i++) drawTab(g, mp, i, t);
    }
  } else {
    // Only the buttons and tabs whose pressed state changed, so a tap
    // doesn't blank the whole pad.
    if (mp.pressed() != pressed_) {
      drawButton(g, mp, pressed_, t);
      drawButton(g, mp, mp.pressed(), t);
    }
    if (mp.pressedTab() != pressedTab_) {
      drawTab(g, mp, pressedTab_, t);
      drawTab(g, mp, mp.pressedTab(), t);
    }
  }
  g.endWrite();
  full_ = false;
  page_ = mp.page();
  pressed_ = mp.pressed();
  pressedTab_ = mp.pressedTab();
  mp.clean();
}

}  // namespace t5m
