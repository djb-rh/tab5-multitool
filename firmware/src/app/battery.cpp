#include "battery.h"

#include <cmath>

namespace mt {
namespace {

constexpr int kBarH = 64;
constexpr uint32_t kBarBg = 0x080A0C, kText = 0xC8D0D8, kGood = 0x7CFC9A, kLow = 0xFF6A5A;
constexpr uint32_t kPollMs = 5000;
constexpr int kLowPercent = 15;

float g_level = -1;  // smoothed: the reading follows voltage, which moves with load
bool g_charging = false;
bool g_read = false;
int g_shown = -1;
bool g_shownCharging = false;
int g_right = -1;

void read() {
  g_read = true;
  const int level = M5.Power.getBatteryLevel();
  if (level < 0) g_level = -1;
  else if (g_level < 0) g_level = level;
  else g_level += (level - g_level) * 0.25f;
  // CHG_STAT does not work on the Tab5; a positive current means charging.
  g_charging = M5.Power.getBatteryCurrent() > 50;
}

int percent() { return g_level < 0 ? -1 : (int)lroundf(g_level); }

}  // namespace

void batteryDraw(lgfx::LGFXBase &d, int right) {
  if (!g_read) read();
  const int pct = percent();
  g_right = right;  // a sprite here is a whole-screen one: same place
  g_shown = pct;
  g_shownCharging = g_charging;
  const int x = right - kBatteryW, y = kBarH / 2 - 10, w = 34, h = 20;
  d.fillRect(x, 12, kBatteryW, kBarH - 24, kBarBg);
  if (pct < 0) return;
  const uint32_t color = g_charging ? kGood : pct <= kLowPercent ? kLow : kText;
  d.drawRoundRect(x, y, w, h, 4, color);
  d.fillRect(x + w, y + 6, 3, h - 12, color);
  const int fill = (w - 6) * (pct > 100 ? 100 : pct) / 100;
  if (fill > 0) d.fillRect(x + 3, y + 3, fill, h - 6, color);
  if (g_charging) {  // a bolt, dark on the fill
    const int cx = x + w / 2, cy = y + h / 2;
    d.fillTriangle(cx + 3, cy - 8, cx - 6, cy + 1, cx + 1, cy + 1, kBarBg);
    d.fillTriangle(cx - 3, cy + 8, cx + 6, cy - 1, cx - 1, cy - 1, kBarBg);
  }
  char s[8];
  snprintf(s, sizeof s, "%d%%", pct);
  d.setFont(&fonts::DejaVu18);
  d.setTextDatum(textdatum_t::middle_right);
  d.setTextColor(color);
  d.drawString(s, right, kBarH / 2);
}

void batteryTick(uint32_t now) {
  static uint32_t last = 0;
  if (now - last < kPollMs) return;
  last = now;
  read();
  if (g_right < 0 || (percent() == g_shown && g_charging == g_shownCharging)) return;
  M5.Display.startWrite();
  batteryDraw(M5.Display, g_right);
  M5.Display.endWrite();
}

}  // namespace mt
