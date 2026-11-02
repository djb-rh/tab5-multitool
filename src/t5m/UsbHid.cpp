#include "UsbHid.h"

#include "ReportDesc.h"

#include <cstring>

#include <esp_mac.h>
#include <esp_system.h>
#include <esp_private/usb_phy.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <hal/usb_serial_jtag_ll.h>
#include <soc/lp_system_struct.h>
#include <tusb.h>

#include <Arduino.h>

namespace t5m {
namespace {

// TinyUSB's port 0 is the P4's full-speed OTG controller (port 1 is the HS one).
constexpr uint8_t kRhport = 0;

enum : uint8_t { kItfHid = 0, kItfCdcCtrl, kItfCdcData, kItfCount };
enum : uint8_t { kEpHid = 0x81, kEpCdcNotif = 0x82, kEpCdcOut = 0x03, kEpCdcIn = 0x83 };

constexpr uint16_t kConfigLenHid = TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN;
constexpr uint16_t kConfigLenFull = kConfigLenHid + TUD_CDC_DESC_LEN;

// Boot subclass on the one HID interface, so BIOS screens and KVMs that only
// speak boot protocol still get a keyboard (sendKeyboard() drops the report
// ID then). The pre-built TinyUSB allows a single HID interface, so mouse and
// media share it under report IDs.
const uint8_t kConfigFull[] = {
    TUD_CONFIG_DESCRIPTOR(1, kItfCount, 0, kConfigLenFull, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(kItfHid, 4, HID_ITF_PROTOCOL_KEYBOARD, sizeof(kHidReportDesc), kEpHid, 16, 1),
    TUD_CDC_DESCRIPTOR(kItfCdcCtrl, 5, kEpCdcNotif, 8, kEpCdcOut, kEpCdcIn, 64),
};
const uint8_t kConfigHid[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, kConfigLenHid, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(kItfHid, 4, HID_ITF_PROTOCOL_KEYBOARD, sizeof(kHidReportDesc), kEpHid, 16, 1),
};

tusb_desc_device_t g_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    // IAD, because of the CDC function.
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A,
    .idProduct = 0x82F0,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

UsbHidConfig g_cfg;
char g_serial[13];
volatile uint8_t g_leds = 0;
usb_phy_handle_t g_phy = nullptr;
TaskHandle_t g_task = nullptr;

// Reports wait here and are sent from the TinyUSB task, one per frame, so a
// press and its release are never merged into nothing.
struct Report {
  uint8_t id;
  uint8_t len;
  uint8_t data[8];
  uint32_t notBefore;  // millis(); 0 = at once
};
QueueHandle_t g_queue = nullptr;

// macOS ignores a Caps Lock tap shorter than about a tenth of a second (its
// guard against brushing the key), and a synthesized tap is ~1 ms. Releases
// of Caps Lock are held back until it has been down this long.
constexpr uint32_t kCapsMinHoldMs = 250;
uint32_t g_capsDownAt = 0;
uint32_t g_notBefore = 0;  // the next report waits until then

void enqueue(uint8_t id, const void *data, uint8_t len) {
  if (!g_queue) return;
  Report r{id, len, {}, g_notBefore};
  g_notBefore = 0;
  memcpy(r.data, data, len);
  // Block briefly when typing outruns the host; drop rather than hang if
  // nobody is listening.
  xQueueSend(g_queue, &r, tud_mounted() && !tud_suspended() ? pdMS_TO_TICKS(100) : 0);
}

void usbTask(void *) {
  Report r;
  bool have = false;
  for (;;) {
    tud_task_ext(1, false);
    if (!tud_mounted()) {
      xQueueReset(g_queue);
      have = false;
      continue;
    }
    if (!have) have = xQueueReceive(g_queue, &r, 0) == pdTRUE;
    if (!have) continue;
    if (tud_suspended()) {
      // A key wakes a sleeping host if it allowed that; the report is kept.
      tud_remote_wakeup();  // does nothing unless the host allowed it
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }
    if (!tud_hid_ready()) continue;
    if (r.notBefore && (int32_t)(millis() - r.notBefore) < 0) continue;
    if (tud_hid_get_protocol() == HID_PROTOCOL_BOOT) {
      // Boot protocol: keyboard only, and without the report ID.
      if (r.id == kReportKeyboard) tud_hid_report(0, r.data, r.len);
    } else {
      tud_hid_report(r.id, r.data, r.len);
    }
    have = false;
  }
}

// USB-C is wired to FSLS PHY 0, which USB-Serial-JTAG owns by default.
void phyToOtg() {
  LP_SYS.usb_ctrl.sw_hw_usb_phy_sel = 1;
  LP_SYS.usb_ctrl.sw_usb_phy_sel = 1;  // USJ -> PHY 1 (not wired), OTG FS -> PHY 0
  USB_SERIAL_JTAG.conf0.usb_pad_enable = 0;  // keep USJ off PHY 1's pins
}
void phyToJtag() {
  LP_SYS.usb_ctrl.sw_usb_phy_sel = 0;
  USB_SERIAL_JTAG.conf0.usb_pad_enable = 1;
}

// LP_SYS is in the low-power domain: the swap survives esp_restart() and a
// panic reset, which would leave USB-C on a controller nobody runs and no
// serial port to flash through. So it is undone on the way down and, in case
// that never ran, again before anything else starts.
void restoreOnShutdown() { phyToJtag(); }
__attribute__((constructor)) void restoreAtBoot() {
  if (LP_SYS.usb_ctrl.sw_usb_phy_sel) phyToJtag();
}

}  // namespace

// ---- UsbHid ----

bool UsbHid::begin(const UsbHidConfig &cfg) {
  if (running_) return true;
  g_cfg = cfg;
  g_device.idVendor = cfg.vid;
  g_device.idProduct = cfg.pid;
  uint8_t mac[6] = {};
  esp_read_mac(mac, ESP_MAC_BASE);
  snprintf(g_serial, sizeof g_serial, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  if (!g_queue) g_queue = xQueueCreate(128, sizeof(Report));

  usb_phy_config_t phy = {};
  phy.controller = USB_PHY_CTRL_OTG;
  phy.target = USB_PHY_TARGET_INT;
  phy.otg_mode = USB_OTG_MODE_DEVICE;
  phy.otg_speed = USB_PHY_SPEED_FULL;
  if (usb_new_phy(&phy, &g_phy) != ESP_OK) return false;
  static bool hooked = false;
  if (!hooked) hooked = esp_register_shutdown_handler(restoreOnShutdown) == ESP_OK;
  phyToOtg();

  tusb_rhport_init_t init = {};
  init.role = TUSB_ROLE_DEVICE;
  init.speed = TUSB_SPEED_FULL;
  if (!tusb_init(kRhport, &init)) {
    usb_del_phy(g_phy);
    g_phy = nullptr;
    phyToJtag();
    return false;
  }
  if (!g_task) xTaskCreatePinnedToCore(usbTask, "t5m_usb", 4096, nullptr, 5, &g_task, 0);
  running_ = true;
  return true;
}

void UsbHid::end() {
  if (!running_) return;
  releaseAll();
  mouseButtons(0);
  vTaskDelay(pdMS_TO_TICKS(30));
  tud_disconnect();
  vTaskDelay(pdMS_TO_TICKS(20));
  if (g_task) {
    vTaskDelete(g_task);
    g_task = nullptr;
  }
  tusb_deinit(kRhport);
  if (g_phy) {
    usb_del_phy(g_phy);
    g_phy = nullptr;
  }
  phyToJtag();
  running_ = false;
}

void UsbHid::sendKeyboard() {
  uint8_t r[8] = {(uint8_t)(key_mods_ | held_mods_), 0};
  memcpy(r + 2, keys_, 6);
  enqueue(kReportKeyboard, r, sizeof r);
}

void UsbHid::press(uint8_t usage) {
  if (usage == key::CapsLock) g_capsDownAt = millis();
  if (usage >= key::LCtrl && usage <= key::RGui) {
    key_mods_ |= 1 << (usage - key::LCtrl);
  } else {
    for (uint8_t k : keys_) if (k == usage) return;
    int slot = -1;
    for (int i = 0; i < 6; i++) if (!keys_[i]) { slot = i; break; }
    if (slot < 0) return;  // seven keys held: the boot report has no room
    keys_[slot] = usage;
  }
  sendKeyboard();
}

void UsbHid::release(uint8_t usage) {
  if (usage == key::CapsLock) g_notBefore = g_capsDownAt + kCapsMinHoldMs;
  if (usage >= key::LCtrl && usage <= key::RGui) {
    key_mods_ &= ~(1 << (usage - key::LCtrl));
  } else {
    bool found = false;
    for (uint8_t &k : keys_) if (k == usage) { k = 0; found = true; }
    if (!found) return;
  }
  sendKeyboard();
}

void UsbHid::releaseAll() {
  memset(keys_, 0, sizeof keys_);
  key_mods_ = held_mods_ = 0;
  sendKeyboard();
}

void UsbHid::setModifiers(uint8_t mods) {
  if (mods == held_mods_) return;
  held_mods_ = mods;
  sendKeyboard();
}

void UsbHid::mouseMove(int dx, int dy, int wheel, int pan) {
  // Report fields are int8; big moves go out as several reports.
  while (dx || dy || wheel || pan) {
    auto step = [](int &v) {
      const int s = v > 127 ? 127 : v < -127 ? -127 : v;
      v -= s;
      return (uint8_t)(int8_t)s;
    };
    const uint8_t r[5] = {buttons_, step(dx), step(dy), step(wheel), step(pan)};
    enqueue(kReportMouse, r, sizeof r);
  }
}

void UsbHid::mouseButtons(uint8_t mask) {
  if (mask == buttons_) return;
  buttons_ = mask;
  const uint8_t r[5] = {buttons_, 0, 0, 0, 0};
  enqueue(kReportMouse, r, sizeof r);
}

void UsbHid::consumer(uint16_t usage) {
  const uint8_t r[2] = {(uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8)};
  enqueue(kReportConsumer, r, sizeof r);
}

bool UsbHid::connected() const { return running_ && tud_mounted() && !tud_suspended(); }
bool UsbHid::mounted() const { return running_ && tud_mounted(); }
bool UsbHid::suspended() const { return running_ && tud_suspended(); }
bool UsbHid::bootProtocol() const { return running_ && tud_hid_get_protocol() == HID_PROTOCOL_BOOT; }
uint8_t UsbHid::leds() const { return g_leds; }

// ---- UsbConsole ----

int UsbConsole::available() { return tud_cdc_available(); }
int UsbConsole::read() { return tud_cdc_available() ? tud_cdc_read_char() : -1; }
int UsbConsole::peek() {
  uint8_t c;
  return tud_cdc_peek(&c) ? c : -1;
}
size_t UsbConsole::write(uint8_t c) { return write(&c, 1); }
size_t UsbConsole::write(const uint8_t *buf, size_t n) {
  if (!open()) return n;
  size_t done = 0;
  for (int spins = 0; done < n && spins < 50; spins++) {
    const uint32_t k = tud_cdc_write(buf + done, n - done);
    done += k;
    tud_cdc_write_flush();
    if (done < n) vTaskDelay(1);
  }
  return n;
}
void UsbConsole::flush() { tud_cdc_write_flush(); }
bool UsbConsole::open() const { return tud_mounted() && tud_cdc_connected(); }

}  // namespace t5m

// ---- TinyUSB callbacks ----

extern "C" {

uint8_t const *tud_descriptor_device_cb(void) { return (uint8_t const *)&t5m::g_device; }

uint8_t const *tud_descriptor_configuration_cb(uint8_t) {
  return t5m::g_cfg.console ? t5m::kConfigFull : t5m::kConfigHid;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t) { return t5m::kHidReportDesc; }

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t) {
  static uint16_t buf[33];
  const char *s;
  switch (index) {
    case 0: buf[1] = 0x0409; buf[0] = (TUSB_DESC_STRING << 8) | 4; return buf;
    case 1: s = t5m::g_cfg.manufacturer; break;
    case 2: s = t5m::g_cfg.product; break;
    case 3: s = t5m::g_serial; break;
    case 4: s = "Keyboard + Mouse"; break;
    case 5: s = "Console"; break;
    default: return nullptr;
  }
  size_t n = strlen(s);
  if (n > 32) n = 32;
  for (size_t i = 0; i < n; i++) buf[1 + i] = (uint8_t)s[i];
  buf[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * n + 2));
  return buf;
}

uint16_t tud_hid_get_report_cb(uint8_t, uint8_t, hid_report_type_t, uint8_t *, uint16_t) { return 0; }

void tud_hid_set_report_cb(uint8_t, uint8_t report_id, hid_report_type_t type, uint8_t const *buf, uint16_t len) {
  if (type != HID_REPORT_TYPE_OUTPUT || len == 0) return;
  // Keyboard LEDs. Depending on the path the report ID may still lead the data.
  if (report_id == t5m::kReportKeyboard || report_id == 0) {
    t5m::g_leds = (len >= 2 && buf[0] == t5m::kReportKeyboard) ? buf[1] : buf[0];
  }
}

}  // extern "C"
