// Sends everything to two sinks at once (USB and Bluetooth, say), so either
// host - or both - get the keys. Host state comes from whichever is connected.
#pragma once

#include "HidSink.h"

namespace t5m {

class HidFanout : public HidSink {
 public:
  HidFanout(HidSink *a, HidSink *b) : a_(a), b_(b) {}
  void press(uint8_t u) override { each([&](HidSink &s) { s.press(u); }); }
  void release(uint8_t u) override { each([&](HidSink &s) { s.release(u); }); }
  void releaseAll() override { each([](HidSink &s) { s.releaseAll(); }); }
  void setModifiers(uint8_t m) override { each([&](HidSink &s) { s.setModifiers(m); }); }
  void mouseMove(int dx, int dy, int w = 0, int p = 0) override { each([&](HidSink &s) { s.mouseMove(dx, dy, w, p); }); }
  void mouseButtons(uint8_t m) override { each([&](HidSink &s) { s.mouseButtons(m); }); }
  void consumer(uint16_t u) override { each([&](HidSink &s) { s.consumer(u); }); }
  bool connected() const override { return (a_ && a_->connected()) || (b_ && b_->connected()); }
  uint8_t leds() const override {
    if (a_ && a_->connected()) return a_->leds();
    return b_ && b_->connected() ? b_->leds() : 0;
  }
  // Either side may be switched off (nullptr) at any time.
  void set(HidSink *a, HidSink *b) { a_ = a; b_ = b; }

 private:
  template <typename F> void each(F f) {
    if (a_) f(*a_);
    if (b_) f(*b_);
  }
  HidSink *a_, *b_;
};

}  // namespace t5m
