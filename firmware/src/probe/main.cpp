// Phase 0 probe: what firmware the radio and keyboard run, what the keyboard's
// raw key mode reports, and how dim its status LEDs can go.
//
// Serial commands:
//   r  report (chip, esp-hosted host/slave versions, keyboard)
//   w  join Wi-Fi again (include/secrets.h)
//   s  scan for Wi-Fi networks
//   u  update the C6's esp-hosted firmware from Espressif's server
//   k  keyboard into raw key mode (0): every press/release is printed
//   h  keyboard into HID mode (1): every modifier/usage pair is printed
//   0..9  LED level test: both LEDs white at brightness N*N (0, 1, 4 ... 81)
//   l  LED colour test: LED0 green, LED1 blue at a low level
#include <M5Unified.h>
#include <WiFi.h>
#include <ESP_HostedOTA.h>
#include <driver/gpio.h>
#include <esp32-hal-hosted.h>

#include "secrets.h"

namespace {

constexpr uint8_t kKbAddr = 0x6D;
constexpr int kKbSda = 0, kKbScl = 1, kKbInt = 50;
constexpr uint32_t kKbFreq = 400000;
constexpr uint8_t kRegIntCfg = 0x00, kRegEventNum = 0x02, kRegBrightness = 0x03;
constexpr uint8_t kRegMode = 0x10, kRegRgbMode = 0x11;
constexpr uint8_t kRegKeyEvent = 0x20, kRegHidEvent = 0x30, kRegRgb = 0x60, kRegFirmware = 0xFE;

bool g_kb = false;
int g_kb_mode = -1;

bool kbAnswers() {
  const bool ok = M5.Ex_I2C.start(kKbAddr, false, kKbFreq) && M5.Ex_I2C.write(kRegFirmware);
  M5.Ex_I2C.stop();
  return ok;
}

void kbBegin() {
  // The keyboard's header has its own pins; the Grove bus is moved over to them.
  M5.Ex_I2C.release();
  M5.Ex_I2C.begin(I2C_NUM_0, kKbSda, kKbScl);
  gpio_config_t io = {};
  io.pin_bit_mask = 1ULL << kKbInt;
  io.mode = GPIO_MODE_INPUT;
  io.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&io);
  g_kb = kbAnswers();
}

void kbSetMode(int mode) {
  if (!g_kb) { Serial.println("keyboard: not found"); return; }
  M5.Ex_I2C.writeRegister8(kKbAddr, kRegMode, mode, kKbFreq);
  M5.Ex_I2C.writeRegister8(kKbAddr, kRegEventNum, 0, kKbFreq);
  M5.Ex_I2C.writeRegister8(kKbAddr, kRegIntCfg, 0x07, kKbFreq);
  g_kb_mode = mode;
  Serial.printf("keyboard: mode %d (read back %u)\n", mode,
                (unsigned)M5.Ex_I2C.readRegister8(kKbAddr, kRegMode, kKbFreq));
}

void kbLeds(uint8_t brightness, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1) {
  if (!g_kb) return;
  const uint8_t buf[8] = {b0, g0, r0, 0, b1, g1, r1, 0};  // B,G,R,pad per LED
  M5.Ex_I2C.writeRegister8(kKbAddr, kRegBrightness, brightness, kKbFreq);
  M5.Ex_I2C.writeRegister(kKbAddr, kRegRgb, buf, sizeof buf, kKbFreq);
  M5.Ex_I2C.writeRegister8(kKbAddr, kRegRgbMode, 1, kKbFreq);  // user colours
}

void kbPoll() {
  if (!g_kb || g_kb_mode < 0) return;
  const uint8_t n = M5.Ex_I2C.readRegister8(kKbAddr, kRegEventNum, kKbFreq);
  if (n == 0 || n > 32) return;
  for (int i = 0; i < n; i++) {
    const uint32_t t = millis();
    if (g_kb_mode == 0) {
      const uint8_t v = M5.Ex_I2C.readRegister8(kKbAddr, kRegKeyEvent, kKbFreq);
      if (v == 0xFF) return;
      Serial.printf("%lu key %s r%u c%u\n", (unsigned long)t, (v & 0x80) ? "DOWN" : "up  ",
                    (unsigned)((v >> 4) & 7), (unsigned)(v & 0x0F));
    } else {
      uint8_t b[2] = {0xFF, 0xFF};
      if (!M5.Ex_I2C.readRegister(kKbAddr, kRegHidEvent, b, 2, kKbFreq)) return;
      if (b[0] == 0xFF && b[1] == 0xFF) return;
      Serial.printf("%lu hid mod %02X usage %02X\n", (unsigned long)t, b[0], b[1]);
    }
  }
}

void wifiBegin() {
  WiFi.mode(WIFI_STA);
  // The default fast scan joins the first AP heard, not the strongest.
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("wifi: joining");
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; ++i) { delay(500); Serial.print('.'); }
  Serial.printf(" %s (status %d, hosted init %d)\n", WiFi.status() == WL_CONNECTED ? "connected" : "FAILED", (int)WiFi.status(), (int)hostedIsInitialized());
}

void report() {
  Serial.println("===== MULTITOOL PROBE =====");
  Serial.printf("arduino core %s, esp-idf %s\n", ESP_ARDUINO_VERSION_STR, esp_get_idf_version());
  uint32_t a, b, c;
  hostedGetHostVersion(&a, &b, &c);
  Serial.printf("esp-hosted host (P4): %lu.%lu.%lu\n", (unsigned long)a, (unsigned long)b, (unsigned long)c);
  if (WiFi.status() == WL_CONNECTED) {
    const bool upd = hostedHasUpdate();
    hostedGetSlaveVersion(&a, &b, &c);
    Serial.printf("esp-hosted slave (%s): %lu.%lu.%lu -> %s\n", hostedGetSlaveTargetName(), (unsigned long)a,
                  (unsigned long)b, (unsigned long)c, upd ? "UPDATE AVAILABLE" : "matches");
    Serial.printf("wifi: ip %s rssi %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println("wifi: not connected");
  }
  if (g_kb) {
    Serial.printf("keyboard: firmware %u, mode %u, rgb mode %u, brightness %u, int %d\n",
                  (unsigned)M5.Ex_I2C.readRegister8(kKbAddr, kRegFirmware, kKbFreq),
                  (unsigned)M5.Ex_I2C.readRegister8(kKbAddr, kRegMode, kKbFreq),
                  (unsigned)M5.Ex_I2C.readRegister8(kKbAddr, kRegRgbMode, kKbFreq),
                  (unsigned)M5.Ex_I2C.readRegister8(kKbAddr, kRegBrightness, kKbFreq),
                  gpio_get_level((gpio_num_t)kKbInt));
  } else {
    Serial.println("keyboard: not found");
  }
  Serial.printf("touch: %d points now\n", (int)M5.Touch.getCount());
  Serial.println("===== END =====");
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  cfg.output_power = false;  // keep the USB-A rail off: it back-drives USB-C VBUS
  M5.begin(cfg);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  M5.Display.setRotation(3);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(3);
  M5.Display.drawString("Tab5 Multitool probe", 40, 40);
  kbBegin();
  if (g_kb) kbLeds(10, 0, 40, 0, 0, 0, 40);
  wifiBegin();
  report();
}

void loop() {
  M5.update();
  kbPoll();
  if (!Serial.available()) { delay(5); return; }
  const int ch = Serial.read();
  if (ch == 'r') report();
  else if (ch == 'w') { WiFi.disconnect(); delay(200); wifiBegin(); report(); }
  else if (ch == 's') {
    const int n = WiFi.scanNetworks();
    Serial.printf("scan: %d networks\n", n);
    for (int i = 0; i < n && i < 12; i++) Serial.printf("  %-32s ch %2d rssi %d\n", WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i));
  }
  else if (ch == 'k') kbSetMode(0);
  else if (ch == 'h') kbSetMode(1);
  else if (ch >= '0' && ch <= '9') {
    const uint8_t v = (uint8_t)((ch - '0') * (ch - '0'));
    kbLeds(v, 255, 255, 255, 255, 255, 255);
    Serial.printf("leds: white, brightness %u\n", (unsigned)v);
  } else if (ch == 'l') {
    kbLeds(10, 0, 40, 0, 0, 0, 40);
    Serial.println("leds: LED0 green, LED1 blue (brightness 10, value 40)");
  } else if (ch == 'u') {
    Serial.println("c6: updating esp-hosted firmware (a minute or two)...");
    const bool ok = updateEspHostedSlave();
    Serial.printf("c6: update %s - power cycle, then 'r'\n", ok ? "OK" : "FAILED");
  }
}
