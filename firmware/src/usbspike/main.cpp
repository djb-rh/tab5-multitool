// Spike S1: the Tab5 as a USB keyboard + mouse on USB-C (full-speed OTG).
//
// Boots as usual (USB-Serial-JTAG, so esptool can flash it), counts down six
// seconds on Serial, then hands USB-C to t5m::UsbHid. From then on the Tab5's
// serial port is the CDC console the HID device carries.
//
// Console commands (either port):
//   s  status    m  draw a small mouse square (ends where it started)
//   f  tap F20 (no action on Mac/PC, but proves keyboard reports)
//   t  type "hello from the Tab5 " (only do this with a text field focused!)
//   j  restart into USB-Serial-JTAG for this boot (to flash it again)
//   x  (during the countdown) stay on USB-Serial-JTAG
//
// Safety net: if no terminal opens the CDC port within 60 s of starting HID,
// it restarts into USB-Serial-JTAG, so a broken USB stack can't strand it.
#include <M5Unified.h>
#include <Tab5Multitool.h>
#include <esp_attr.h>

namespace {

constexpr uint32_t kStayJtagMagic = 0x4A544147;  // "JTAG"
RTC_NOINIT_ATTR uint32_t g_stay_jtag;

t5m::UsbHid g_hid;
uint32_t g_hid_started = 0;
bool g_console_seen = false;

void show(const char *line1, const char *line2 = "") {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(4);
  M5.Display.drawString("Tab5 Multitool - USB spike", 30, 30);
  M5.Display.drawString(line1, 30, 120);
  M5.Display.drawString(line2, 30, 180);
}

void restartToJtag() {
  g_stay_jtag = kStayJtagMagic;
  g_hid.console().println("restarting into USB-Serial-JTAG");
  g_hid.console().flush();
  delay(100);
  esp_restart();
}

void status(Print &out) {
  out.printf("usb: running %d mounted %d suspended %d boot-protocol %d leds 0x%02X console %d\n",
             g_hid.running(), g_hid.mounted(), g_hid.suspended(), g_hid.bootProtocol(), g_hid.leds(),
             g_hid.console().open());
}

void command(int ch, Print &out) {
  if (ch == 's') status(out);
  else if (ch == 'm') {
    for (int side = 0; side < 4; side++) {
      const int dx[] = {40, 0, -40, 0}, dy[] = {0, 40, 0, -40};
      for (int i = 0; i < 10; i++) { g_hid.mouseMove(dx[side] / 10, dy[side] / 10); delay(10); }
    }
    out.println("mouse: square done");
  } else if (ch == 'f') {
    g_hid.tap(t5m::key::f(20));
    out.println("key: F20 tapped");
  } else if (ch == 't') {
    out.printf("typed %d chars\n", g_hid.typeText("hello from the Tab5 "));
  } else if (ch == 'j') restartToJtag();
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  cfg.output_power = false;  // keep the USB-A rail off: it back-drives USB-C VBUS
  M5.begin(cfg);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  M5.Display.setRotation(3);

  if (g_stay_jtag == kStayJtagMagic) {
    g_stay_jtag = 0;
    show("Staying on USB-Serial-JTAG", "(flash it now)");
    Serial.println("usbspike: staying on USB-Serial-JTAG this boot");
    return;
  }
  for (int i = 6; i > 0; i--) {
    char buf[48];
    snprintf(buf, sizeof buf, "USB HID in %d s (x = stay)", i);
    show(buf);
    Serial.printf("usbspike: %s\n", buf);
    for (int k = 0; k < 10; k++) {
      delay(100);
      if (Serial.available() && Serial.read() == 'x') {
        show("Staying on USB-Serial-JTAG");
        Serial.println("usbspike: staying");
        return;
      }
    }
  }
  Serial.println("usbspike: starting USB HID, Serial goes quiet now");
  Serial.flush();
  delay(50);
  const bool ok = g_hid.begin();
  g_hid_started = millis();
  show(ok ? "USB HID started" : "USB HID FAILED to start");
}

void loop() {
  M5.update();
  if (!g_hid.running()) {
    if (Serial.available()) command(Serial.read(), Serial);
    delay(10);
    return;
  }
  auto &con = g_hid.console();
  if (con.open() && !g_console_seen) {
    g_console_seen = true;
    con.println("usbspike: console open");
    status(con);
  }
  if (!g_console_seen && millis() - g_hid_started > 60000) restartToJtag();
  while (con.available()) command(con.read(), con);

  static uint32_t last = 0;
  if (millis() - last > 500) {
    last = millis();
    char l2[64];
    snprintf(l2, sizeof l2, "mounted %d  suspended %d  leds %02X  console %d", g_hid.mounted(), g_hid.suspended(),
             g_hid.leds(), con.open());
    M5.Display.fillRect(0, 170, 1280, 60, TFT_BLACK);
    M5.Display.drawString(l2, 30, 180);
  }
  delay(5);
}
