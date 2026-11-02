// USB serial adapters on the Tab5's USB-A port (USB host).
//
// Drivers: CDC-ACM, FTDI (FT232R/FT231X/FT232H/FT2232H/FT4232H), Prolific
// PL2303 (HX and older), Silicon Labs CP210x, WCH CH340/CH341. Setup sequences
// follow the Linux usb-serial drivers.
#pragma once

#include <cstddef>
#include <cstdint>

namespace mt {
namespace usbserial {

struct Line {
  uint32_t baud = 115200;
  uint8_t dataBits = 8;  // 5..8
  uint8_t parity = 0;    // 0 none, 1 odd, 2 even, 3 mark, 4 space
  uint8_t stopBits = 0;  // 0 one, 1 one and a half, 2 two
  bool rtscts = false;   // hardware flow control (FTDI, CP210x)
};

// Powers the USB-A port and starts the host stack (once; later calls just
// power the port again).
void begin();
// Lets go of any adapter and powers the USB-A port off.
void end();
// Applies new line settings (now, and to adapters plugged in later).
void setLine(const Line &line);
const Line &line();

// The adapter in use, if any: "FTDI FT232R", "CH340", ... or nullptr.
const char *adapter();
// Changes each time an adapter comes or goes, or fails to set up.
uint32_t generation();
// Why the last device was not used ("no serial interface", ...), or "".
const char *problem();

// Bytes received; returns how many were copied.
size_t read(uint8_t *buf, size_t cap);
// Queues bytes to send; returns how many fit.
size_t write(const uint8_t *data, size_t n);

}  // namespace usbserial
}  // namespace mt
