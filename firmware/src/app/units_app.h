// The Units app: converts between units of 24 kinds (units.h), typed on the
// clip-on keyboard or an on-screen number pad.
#pragma once

#include <Tab5Multitool.h>

namespace mt {

// kb is the clip-on keyboard reader main.cpp already ticks.
void unitsBegin(t5m::Tab5Keyboard &kb, const t5m::Theme &theme);
void unitsLoop(uint32_t now);
// Leaving for the home screen: saves the kind, units and value.
void unitsEnd();
// Console testing: a character as if typed, and a tap at (x, y).
void unitsType(char c);
void unitsTap(int x, int y);

// Provided by main.cpp: back to the home screen (after this loop pass).
void appGoHome();

}  // namespace mt
