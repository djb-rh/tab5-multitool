// The Wi-Fi network the Tab5 joins, and the screen for choosing it (from the
// home screen's Wi-Fi button). The network is saved in NVS; with none saved,
// a development build that has include/secrets.h uses that one.
#pragma once

#include <Tab5Multitool.h>

namespace mt {

// The saved network: read at boot, before anything wants Wi-Fi.
void wifiLoad();
// The network to join ("" if none), and its password.
const char *wifiSsid();
const char *wifiPass();
// Joins the network, if Wi-Fi is on in the settings and there is one.
void wifiConnect();

// The Wi-Fi screen. kb is the clip-on keyboard reader main.cpp already ticks.
void wifiScreenBegin(t5m::Tab5Keyboard &kb, const t5m::Theme &theme);
void wifiScreenLoop(uint32_t now);
// Console testing: a character as if typed, and a tap at (x, y).
void wifiScreenType(char c);
void wifiScreenTap(int x, int y);

// Provided by main.cpp: back to the home screen (after this loop pass).
void appGoHome();

}  // namespace mt
