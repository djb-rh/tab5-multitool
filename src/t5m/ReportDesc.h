// The one HID report map both transports present: keyboard (ID 1, boot
// layout: modifiers, reserved, six keys; LEDs as output), mouse (ID 2:
// buttons, X, Y, wheel, pan), consumer control (ID 3: one 16-bit usage).
#pragma once

#include <cstdint>

#include <tusb.h>  // only for the report-map macros

namespace t5m {
enum : uint8_t { kReportKeyboard = 1, kReportMouse = 2, kReportConsumer = 3 };
inline constexpr uint8_t kHidReportDesc[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(kReportKeyboard)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(kReportMouse)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(kReportConsumer)),
};
}  // namespace t5m
