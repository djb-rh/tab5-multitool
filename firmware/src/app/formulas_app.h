// The Formulas app: a library of scientific formulas (formulas.h) to find, fill in
// and solve for any variable, with diagrams, unit help and unit conversion.
#pragma once

#include <Tab5Multitool.h>

namespace mt {

// kb is the clip-on keyboard reader main.cpp already ticks.
void formulasBegin(t5m::Tab5Keyboard &kb, const t5m::Theme &theme);
void formulasLoop(uint32_t now);
// Leaving for the home screen: saves favourites and recent formulas.
void formulasEnd();
// Console testing: a character as if typed, and a tap at (x, y).
void formulasType(char c);
void formulasTap(int x, int y);
// Console testing: draws the next page of every diagram in the library.
void formulasGallery();

// Provided by main.cpp: back to the home screen (after this loop pass).
void appGoHome();

}  // namespace mt
