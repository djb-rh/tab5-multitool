// The Terminal app: SwiftTerm over SSH or a USB serial adapter.
#pragma once

#include <Tab5Multitool.h>

namespace mt {

// kb is the clip-on keyboard reader main.cpp already ticks.
void terminalBegin(t5m::Tab5Keyboard &kb, const t5m::Theme &theme);
void terminalLoop(uint32_t now);
// Leaving for the home screen: ends SSH, powers USB-A off. terminalBegin()
// may follow later in the same boot; the screen's contents are kept.
void terminalEnd();
// Feeds bytes to the screen as if the connection sent them (console testing).
void terminalFeed(const uint8_t *data, int n);
// Types text as if from the keyboard (console testing).
void terminalType(const char *text);

// Provided by main.cpp: back to the home screen (after this loop pass).
void appGoHome();

}  // namespace mt
