// Tab5 Multitool: the M5Stack Tab5 as a keyboard / mouse / macropad for
// another computer, in pieces any Tab5 sketch can use. MIT licence.
//
//   UsbHid        USB keyboard + mouse + media keys on USB-C (HidSink)
//   BleHid        the same over Bluetooth LE (through the C6)
//   HidFanout     both at once
//   Tab5Keyboard  the clip-on Tab5 Keyboard, real key up/down, status LEDs
//   Keypad        on-screen keyboards (QWERTY, Fn/symbols, numpad, nav/media)
//   Trackpad      on-screen touchpad
//   KeyText       key events -> characters, for local text entry
//   Macro*        macropad layouts (JSON), MacroPad widget, MacroRunner
//   RenderM5GFX   default drawing for Keypad and Trackpad
#pragma once

#include "t5m/hid_usage.h"
#include "t5m/BleHid.h"
#include "t5m/HidFanout.h"
#include "t5m/HidSink.h"
#include "t5m/Keypad.h"
#include "t5m/Macro.h"
#include "t5m/MacroPad.h"
#include "t5m/RenderM5GFX.h"
#include "t5m/Tab5Keyboard.h"
#include "t5m/Touch.h"
#include "t5m/Trackpad.h"
#include "t5m/UsbHid.h"
