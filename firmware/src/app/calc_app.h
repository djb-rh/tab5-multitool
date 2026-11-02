// The Calculator app: an HP-style RPN scientific calculator (rpn.h) with a
// touch keypad and the clip-on keyboard.
#pragma once

#include <Tab5Multitool.h>

namespace mt {

// kb is the clip-on keyboard reader main.cpp already ticks.
void calcBegin(t5m::Tab5Keyboard &kb, const t5m::Theme &theme);
void calcLoop(uint32_t now);
// Leaving for the home screen: saves the stack and registers.
void calcEnd();
// Console testing: a character as if typed, and a tap at (x, y).
void calcType(char c);
void calcTap(int x, int y);

// Provided by main.cpp: back to the home screen (after this loop pass).
void appGoHome();

}  // namespace mt
