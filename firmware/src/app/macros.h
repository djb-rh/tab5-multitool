// The macropad's layout: kept as JSON on LittleFS (/macros.json), parsed
// into a t5m::MacroLayout. Without a saved file the library's example shows.
#pragma once

#include <Tab5Multitool.h>

#include <string>

namespace mt {

void macrosBegin();
const t5m::MacroLayout &macros();
// The JSON as saved (or the example), for the web editor.
const std::string &macrosJson();
// Validates, then saves and switches to it. On error nothing changes.
bool macrosSave(const std::string &json, std::string *error);
// Back to the example (deletes the saved file).
void macrosReset();
// Bumped on every change, so the screen knows to redraw.
uint32_t macrosVersion();

}  // namespace mt
