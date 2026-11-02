// The Tab5 as a USB keyboard + mouse + media keys on its USB-C socket.
//
// The ESP32-P4 has two USB controllers. The high-speed one drives the Tab5's
// USB-A socket (where a T48 or a gamepad goes), and Arduino's own USB stack
// insists on it. This uses the full-speed one instead, running TinyUSB on
// it directly, so USB-A stays free for anything else.
//
// The USB-C socket is normally USB-Serial-JTAG (Serial, flashing). The P4's
// two full-speed PHYs are swapped so the OTG controller gets the one wired to
// USB-C. While this runs, Serial goes nowhere: the device brings its own CDC
// serial port instead (console()). end() puts USB-Serial-JTAG back.
// Download mode (hold the power button) always flashes, whatever is running.
#pragma once

#include <Stream.h>

#include "HidSink.h"

namespace t5m {

struct UsbHidConfig {
  const char *product = "Tab5 Multitool";
  const char *manufacturer = "Tab5 Multitool";
  uint16_t vid = 0x303A;  // Espressif; the PID is a development one
  uint16_t pid = 0x82F0;
  bool console = true;    // add a CDC serial port next to the HID interface
};

// The CDC serial port, for logging and commands while USB-Serial-JTAG is gone.
// Writes are dropped while no terminal has the port open.
class UsbConsole : public Stream {
 public:
  int available() override;
  int read() override;
  int peek() override;
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buf, size_t n) override;
  void flush() override;
  bool open() const;  // a terminal has the port open (DTR set)
  using Print::write;
};

class UsbHid : public HidSink {
 public:
  bool begin(const UsbHidConfig &cfg = UsbHidConfig());
  // Disconnects from the host and gives USB-C back to USB-Serial-JTAG.
  void end();
  bool running() const { return running_; }

  void press(uint8_t usage) override;
  void release(uint8_t usage) override;
  void releaseAll() override;
  void setModifiers(uint8_t mods) override;
  void mouseMove(int dx, int dy, int wheel = 0, int pan = 0) override;
  void mouseButtons(uint8_t mask) override;
  void consumer(uint16_t usage) override;
  bool connected() const override;
  uint8_t leds() const override;

  bool mounted() const;    // enumerated by a host
  bool suspended() const;  // host asleep (or the cable carries no data)
  bool bootProtocol() const;  // host asked for boot protocol (BIOS, KVM)
  UsbConsole &console() { return console_; }

 private:
  void sendKeyboard();
  bool running_ = false;
  uint8_t keys_[6] = {};
  uint8_t key_mods_ = 0;   // from modifier usages pressed
  uint8_t held_mods_ = 0;  // from setModifiers()
  uint8_t buttons_ = 0;
  UsbConsole console_;
};

}  // namespace t5m
