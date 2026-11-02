// Anything that can send keyboard/mouse/media reports to a host: USB, BLE, or
// a fan-out to several. Widgets and the keyboard bridge talk to this.
#pragma once

#include <cstdint>

#include "hid_usage.h"

namespace t5m {

class HidSink {
 public:
  virtual ~HidSink() = default;

  // Keyboard. Up to six keys are held at once (boot keyboard rollover);
  // modifier usages (0xE0..0xE7) set modifier bits instead of a slot.
  virtual void press(uint8_t usage) = 0;
  virtual void release(uint8_t usage) = 0;
  virtual void releaseAll() = 0;
  // Modifier bits held on their own, on top of any modifier keys pressed.
  virtual void setModifiers(uint8_t mods) = 0;

  // Mouse: relative motion, wheel (+ = away from the user), pan (+ = right).
  virtual void mouseMove(int dx, int dy, int wheel = 0, int pan = 0) = 0;
  virtual void mouseButtons(uint8_t mask) = 0;

  // One consumer-page usage (volume, media); 0 releases it.
  virtual void consumer(uint16_t usage) = 0;

  // Host state.
  virtual bool connected() const = 0;
  virtual uint8_t leds() const = 0;  // led::NumLock | led::CapsLock | ...

  // Conveniences built on the above.
  void apply(const KeyEvent &e);
  void tap(uint8_t usage, uint8_t mods = 0);
  void consumerTap(uint16_t usage);
  void click(uint8_t buttons = button::Left);
  // Types printable ASCII plus \n and \t on a US-layout host; returns the
  // number of characters sent. Characters with no key are skipped.
  int typeText(const char *text);
};

}  // namespace t5m
