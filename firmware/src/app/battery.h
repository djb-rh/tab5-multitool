// The battery badge every app shows in its top bar: an icon and the percentage,
// drawn the same way everywhere, repainted here when the level changes.
#pragma once

#include <M5Unified.h>

namespace mt {

constexpr int kBatteryW = 104;  // room the badge takes, left of `right`

// Draws the badge in a top bar, on d, ending at x = right. The screen's last
// position is remembered for batteryTick(), so call this from each bar drawing.
void batteryDraw(lgfx::LGFXBase &d, int right);
// Call from loop(): reads the battery every few seconds, redraws on a change.
void batteryTick(uint32_t now);

}  // namespace mt
