// The Tab5 as a Bluetooth LE keyboard + mouse + media keys (HID over GATT).
//
// The P4 has no radio; Bluetooth goes through the Tab5's ESP32-C6 over
// esp-hosted (NimBLE on the P4, the controller on the C6), so the C6 needs
// esp-hosted 2.x slave firmware. Pairing is "Just Works" with bonding, so a
// host reconnects by itself after either side restarts.
#pragma once

#include <Print.h>
#include <WString.h>

#include "HidSink.h"

namespace t5m {

struct BleHidConfig {
  const char *name = "Tab5 Multitool";
  const char *manufacturer = "Tab5 Multitool";
  uint8_t battery = 100;
  Print *log = nullptr;  // connection / pairing events, if given
  // The Bluetooth address is a random static one made from the chip's MAC and
  // this number. A host keeps keys per address, so a new number is a new
  // device to every host - the way out when a host holds a stale bond it
  // won't let you forget (macOS can't forget a device that never finished
  // pairing). Keep it in NVS; 0 uses the chip's public address.
  uint8_t identity = 1;
};

class BleHid : public HidSink {
 public:
  bool begin(const BleHidConfig &cfg = BleHidConfig());
  bool running() const { return running_; }
  void setBattery(uint8_t percent);
  // Forget every bonded host (they must pair again). To make hosts forget
  // this device too, also change BleHidConfig::identity and restart.
  void forgetHosts();
  // The address in use, "aa:bb:cc:dd:ee:ff".
  String address() const;

  void press(uint8_t usage) override;
  void release(uint8_t usage) override;
  void releaseAll() override;
  void setModifiers(uint8_t mods) override;
  void mouseMove(int dx, int dy, int wheel = 0, int pan = 0) override;
  void mouseButtons(uint8_t mask) override;
  void consumer(uint16_t usage) override;
  bool connected() const override;
  uint8_t leds() const override;

 private:
  void sendKeyboard();
  bool running_ = false;
  uint8_t keys_[6] = {};
  uint8_t key_mods_ = 0, held_mods_ = 0, buttons_ = 0;
};

}  // namespace t5m
