#include "BleHid.h"

#include <cstring>

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLESecurity.h>
#include <BLEServer.h>
#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#include <esp_mac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "ReportDesc.h"

namespace t5m {
namespace {

volatile bool g_connected = false;
volatile uint16_t g_conn = 0;
volatile uint8_t g_leds = 0;
bool g_random = false;  // advertising from the random static address

struct Report {
  uint8_t id, len, data[8];
  uint32_t notBefore;  // millis(); 0 = at once
};
QueueHandle_t g_queue = nullptr;
TaskHandle_t g_task = nullptr;

// macOS ignores a Caps Lock tap shorter than about a tenth of a second (its
// guard against brushing the key), and a synthesized tap is ~1 ms. Releases
// of Caps Lock are held back until it has been down this long.
constexpr uint32_t kCapsMinHoldMs = 250;
uint32_t g_capsDownAt = 0;
uint32_t g_notBefore = 0;  // the next report waits until then

// ---- The GATT table, registered with NimBLE directly ----
//
// Not BLEHIDDevice: the core's BLEService won't hold two characteristics with
// the same UUID under NimBLE (addCharacteristic just revives the first), and
// HID over GATT needs one Report characteristic (0x2A4D) per report. Only the
// keyboard report ever reached the table, and the first mouse report hit
// BLECharacteristic::notify()'s getService() assert.

struct ReportState {
  uint8_t id, type;  // type: 1 input, 2 output
  uint8_t value[8];
  uint8_t len;
  uint16_t handle;   // value handle, filled in by NimBLE
};
ReportState g_reports[4] = {
    {kReportKeyboard, 1, {}, 8, 0},
    {kReportMouse, 1, {}, 5, 0},
    {kReportConsumer, 1, {}, 2, 0},
    {kReportKeyboard, 2, {}, 1, 0},  // the keyboard LEDs, written by the host
};
char g_manufacturer[32] = "Tab5 Multitool";
uint8_t g_battery = 100;
uint16_t g_batteryHandle = 0;
uint8_t g_protocolMode = 1;  // report protocol

const uint8_t kHidInfo[4] = {0x11, 0x01, 0x00, 0x02};  // HID 1.11, no country, normally connectable
// PnP ID is little-endian: source 2 = USB IF, then VID, PID, version.
const uint8_t kPnp[7] = {0x02, 0x3A, 0x30, 0xF0, 0x82, 0x00, 0x01};

int readFlat(ble_gatt_access_ctxt *ctxt, const void *data, size_t len) {
  return os_mbuf_append(ctxt->om, data, len) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

enum : uintptr_t { kHidInfoArg = 100, kReportMapArg, kControlArg, kProtocolArg, kManufacturerArg, kPnpArg, kBatteryArg };

int accessChr(uint16_t, uint16_t, ble_gatt_access_ctxt *ctxt, void *arg) {
  const uintptr_t a = (uintptr_t)arg;
  const bool read = ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR;
  if (a < 4) {
    ReportState &r = g_reports[a];
    if (read) return readFlat(ctxt, r.value, r.len);
    const uint16_t n = OS_MBUF_PKTLEN(ctxt->om);
    uint8_t buf[8] = {};
    ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof buf, nullptr);
    if (r.type == 2 && n > 0) g_leds = buf[0];
    return 0;
  }
  switch (a) {
    case kHidInfoArg: return readFlat(ctxt, kHidInfo, sizeof kHidInfo);
    case kReportMapArg: return readFlat(ctxt, kHidReportDesc, sizeof kHidReportDesc);
    case kControlArg: return 0;  // suspend / exit suspend: nothing to do
    case kProtocolArg:
      if (read) return readFlat(ctxt, &g_protocolMode, 1);
      ble_hs_mbuf_to_flat(ctxt->om, &g_protocolMode, 1, nullptr);
      return 0;
    case kManufacturerArg: return readFlat(ctxt, g_manufacturer, strlen(g_manufacturer));
    case kPnpArg: return readFlat(ctxt, kPnp, sizeof kPnp);
    case kBatteryArg: return readFlat(ctxt, &g_battery, 1);
  }
  return BLE_ATT_ERR_UNLIKELY;
}

// Report Reference descriptors (0x2908): report ID and type.
int accessRef(uint16_t, uint16_t, ble_gatt_access_ctxt *ctxt, void *arg) {
  const ReportState &r = g_reports[(uintptr_t)arg];
  const uint8_t ref[2] = {r.id, r.type};
  return readFlat(ctxt, ref, sizeof ref);
}

const ble_uuid16_t kUuidHid = BLE_UUID16_INIT(0x1812), kUuidDis = BLE_UUID16_INIT(0x180A),
                   kUuidBattery = BLE_UUID16_INIT(0x180F), kUuidReport = BLE_UUID16_INIT(0x2A4D),
                   kUuidReportRef = BLE_UUID16_INIT(0x2908), kUuidHidInfo = BLE_UUID16_INIT(0x2A4A),
                   kUuidReportMap = BLE_UUID16_INIT(0x2A4B), kUuidControl = BLE_UUID16_INIT(0x2A4C),
                   kUuidProtocol = BLE_UUID16_INIT(0x2A4E), kUuidManufacturer = BLE_UUID16_INIT(0x2A29),
                   kUuidPnp = BLE_UUID16_INIT(0x2A50), kUuidBatteryLevel = BLE_UUID16_INIT(0x2A19);

// NimBLE keeps pointers into these, so they live for the program.
ble_gatt_dsc_def g_refs[4][2];
ble_gatt_chr_def g_hidChrs[9], g_disChrs[3], g_batChrs[2];
ble_gatt_svc_def g_svcs[4];

int registerGatt() {
  memset(g_refs, 0, sizeof g_refs);
  memset(g_hidChrs, 0, sizeof g_hidChrs);
  memset(g_disChrs, 0, sizeof g_disChrs);
  memset(g_batChrs, 0, sizeof g_batChrs);
  memset(g_svcs, 0, sizeof g_svcs);
  auto chr = [](ble_gatt_chr_def &c, const ble_uuid16_t &u, uintptr_t arg, ble_gatt_chr_flags f, uint16_t *h) {
    c.uuid = &u.u;
    c.access_cb = accessChr;
    c.arg = (void *)arg;
    c.flags = f;
    c.val_handle = h;
  };
  int n = 0;
  chr(g_hidChrs[n++], kUuidHidInfo, kHidInfoArg, BLE_GATT_CHR_F_READ, nullptr);
  chr(g_hidChrs[n++], kUuidReportMap, kReportMapArg, BLE_GATT_CHR_F_READ, nullptr);
  chr(g_hidChrs[n++], kUuidControl, kControlArg, BLE_GATT_CHR_F_WRITE_NO_RSP, nullptr);
  chr(g_hidChrs[n++], kUuidProtocol, kProtocolArg, BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP, nullptr);
  for (uintptr_t i = 0; i < 4; i++) {
    // Reports want an encrypted link; asking for one is what makes a host pair.
    const ble_gatt_chr_flags f = g_reports[i].type == 1
        ? BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY
        : BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC |
              BLE_GATT_CHR_F_WRITE_NO_RSP;
    chr(g_hidChrs[n], kUuidReport, i, f, &g_reports[i].handle);
    g_refs[i][0].uuid = &kUuidReportRef.u;
    g_refs[i][0].att_flags = BLE_ATT_F_READ;
    g_refs[i][0].access_cb = accessRef;
    g_refs[i][0].arg = (void *)i;
    g_hidChrs[n++].descriptors = g_refs[i];
  }
  chr(g_disChrs[0], kUuidManufacturer, kManufacturerArg, BLE_GATT_CHR_F_READ, nullptr);
  chr(g_disChrs[1], kUuidPnp, kPnpArg, BLE_GATT_CHR_F_READ, nullptr);
  chr(g_batChrs[0], kUuidBatteryLevel, kBatteryArg, BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY, &g_batteryHandle);

  g_svcs[0] = {BLE_GATT_SVC_TYPE_PRIMARY, &kUuidDis.u, nullptr, g_disChrs};
  g_svcs[1] = {BLE_GATT_SVC_TYPE_PRIMARY, &kUuidHid.u, nullptr, g_hidChrs};
  g_svcs[2] = {BLE_GATT_SVC_TYPE_PRIMARY, &kUuidBattery.u, nullptr, g_batChrs};
  int rc = ble_gatts_count_cfg(g_svcs);
  if (rc == 0) rc = ble_gatts_add_svcs(g_svcs);
  return rc;
}

void notifyReport(uint8_t id, const uint8_t *data, uint8_t len) {
  ReportState *r = nullptr;
  for (auto &s : g_reports) if (s.id == id && s.type == 1) r = &s;
  if (!r || !r->handle) return;
  memcpy(r->value, data, len);
  r->len = len;
  os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
  if (om) ble_gatts_notify_custom(g_conn, r->handle, om);
}

void enqueue(uint8_t id, const void *data, uint8_t len) {
  if (!g_queue || !g_connected) return;
  Report r{id, len, {}, g_notBefore};
  g_notBefore = 0;
  memcpy(r.data, data, len);
  xQueueSend(g_queue, &r, pdMS_TO_TICKS(100));
}

// Notifications go out from one task, a little apart: a burst faster than
// the connection interval is otherwise dropped by some hosts.
void bleTask(void *) {
  Report r;
  for (;;) {
    if (xQueueReceive(g_queue, &r, portMAX_DELAY) != pdTRUE) continue;
    if (!g_connected || r.id < 1 || r.id > 3) continue;
    while (r.notBefore && (int32_t)(millis() - r.notBefore) < 0) vTaskDelay(pdMS_TO_TICKS(5));
    notifyReport(r.id, r.data, r.len);
    vTaskDelay(pdMS_TO_TICKS(8));
  }
}

Print *g_log = nullptr;
template <typename... A> void logf(const char *fmt, A... a) {
  if (g_log) g_log->printf(fmt, a...);
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *, ble_gap_conn_desc *desc) override {
    g_conn = desc->conn_handle;
    g_connected = true;
    logf("ble: connected (handle %u, encrypted %d, bonded %d)\n", desc->conn_handle, desc->sec_state.encrypted,
         desc->sec_state.bonded);
  }
  void onDisconnect(BLEServer *, ble_gap_conn_desc *desc) override {
    g_connected = false;
    if (g_queue) xQueueReset(g_queue);
    logf("ble: disconnected (handle %u)\n", desc->conn_handle);
    BLEDevice::startAdvertising();
  }
};

class SecurityCallbacks : public BLESecurityCallbacks {
  bool onSecurityRequest() override { return true; }
  bool onConfirmPIN(uint32_t) override { return true; }
  uint32_t onPassKeyRequest() override { return 0; }
  void onPassKeyNotify(uint32_t) override {}
  // The library calls this one (with NimBLE's status) on every encryption change.
  void onAuthenticationComplete(ble_gap_conn_desc *desc, int status) override {
    if (status == 0) {
      logf("ble: encrypted (bonded %d, key size %u)\n", desc->sec_state.bonded, desc->sec_state.key_size);
      return;
    }
    // NimBLE: 0x400 + SMP reason = our side failed it, 0x500 + reason = the
    // host's side, 0x200 + code = HCI.
    const char *who = status >= 0x400 && status < 0x500 ? "tab5" : status >= 0x500 && status < 0x600 ? "host"
                    : status >= 0x200 && status < 0x300 ? "hci" : "other";
    logf("ble: pairing FAILED status %d (0x%03x, %s reason %d)\n", status, status, who, status & 0xFF);
  }
  void onAuthenticationComplete(ble_gap_conn_desc *desc) override { onAuthenticationComplete(desc, 0); }
};

}  // namespace

bool BleHid::begin(const BleHidConfig &cfg) {
  if (running_) return true;
  g_log = cfg.log;
  if (!BLEDevice::init(cfg.name)) return false;
  if (cfg.identity) {
    // Random static: made from the MAC so it is stable, top two bits set as
    // the spec requires. NimBLE wants it least significant byte first.
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BASE);  // the P4 has no BT MAC (its radio is the C6)
    uint8_t addr[6];
    for (int i = 0; i < 6; i++) addr[i] = mac[5 - i];
    addr[0] ^= cfg.identity;
    addr[3] ^= (uint8_t)(cfg.identity * 0x5B);
    addr[5] |= 0xC0;
    g_random = BLEDevice::setOwnAddr(addr) && BLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
    if (!g_random) logf("ble: could not set the random address, using the public one\n");
  }
  logf("ble: address %s (identity %u)\n", address().c_str(), cfg.identity);
  // The constructor sets which keys are exchanged when bonding; the static
  // setters alone leave that at none, and hosts then drop the pairing.
  static BLESecurity *security = new BLESecurity();
  security->setCapability(ESP_IO_CAP_NONE);
  // Bond, no MITM, and LE Legacy pairing rather than Secure Connections: with
  // NimBLE over esp-hosted on the P4, SC pairing fails our own DHKey check
  // (status 0x40B) against macOS. Legacy "Just Works" is what many BLE
  // keyboards use, and hosts accept it.
  security->setAuthenticationMode(true, false, false);
  BLEDevice::setSecurityCallbacks(new SecurityCallbacks());

  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  // createServer() has just reset the GATT table; the server starts with the
  // first advertising, so our services go in between.
  strncpy(g_manufacturer, cfg.manufacturer, sizeof g_manufacturer - 1);
  g_battery = cfg.battery;
  const int rc = registerGatt();
  if (rc != 0) {
    logf("ble: GATT registration failed, rc=%d\n", rc);
    return false;
  }

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->setAppearance(0x03C1);  // HID keyboard
  adv->addServiceUUID(BLEUUID((uint16_t)0x1812));
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);
  adv->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

  if (!g_queue) g_queue = xQueueCreate(128, sizeof(Report));
  if (!g_task) xTaskCreatePinnedToCore(bleTask, "t5m_ble", 4096, nullptr, 4, &g_task, 0);
  running_ = true;
  return true;
}

void BleHid::setBattery(uint8_t percent) {
  g_battery = percent > 100 ? 100 : percent;
  if (g_connected && g_batteryHandle) ble_gatts_notify(g_conn, g_batteryHandle);
}

String BleHid::address() const {
  uint8_t a[6] = {};
  ble_hs_id_copy_addr(g_random ? BLE_OWN_ADDR_RANDOM : BLE_OWN_ADDR_PUBLIC, a, nullptr);
  char buf[18];
  snprintf(buf, sizeof buf, "%02x:%02x:%02x:%02x:%02x:%02x", a[5], a[4], a[3], a[2], a[1], a[0]);
  return String(buf);
}

void BleHid::forgetHosts() {
  // NimBLE keeps bonds in NVS; deleting them all is the portable way.
  ble_store_clear();
}

void BleHid::sendKeyboard() {
  uint8_t r[8] = {(uint8_t)(key_mods_ | held_mods_), 0};
  memcpy(r + 2, keys_, 6);
  enqueue(kReportKeyboard, r, sizeof r);
}

void BleHid::press(uint8_t usage) {
  if (usage == key::CapsLock) g_capsDownAt = millis();
  if (usage >= key::LCtrl && usage <= key::RGui) {
    key_mods_ |= 1 << (usage - key::LCtrl);
  } else {
    for (uint8_t k : keys_) if (k == usage) return;
    int slot = -1;
    for (int i = 0; i < 6; i++) if (!keys_[i]) { slot = i; break; }
    if (slot < 0) return;
    keys_[slot] = usage;
  }
  sendKeyboard();
}

void BleHid::release(uint8_t usage) {
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

void BleHid::releaseAll() {
  memset(keys_, 0, sizeof keys_);
  key_mods_ = held_mods_ = 0;
  sendKeyboard();
}

void BleHid::setModifiers(uint8_t mods) {
  if (mods == held_mods_) return;
  held_mods_ = mods;
  sendKeyboard();
}

void BleHid::mouseMove(int dx, int dy, int wheel, int pan) {
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

void BleHid::mouseButtons(uint8_t mask) {
  if (mask == buttons_) return;
  buttons_ = mask;
  const uint8_t r[5] = {buttons_, 0, 0, 0, 0};
  enqueue(kReportMouse, r, sizeof r);
}

void BleHid::consumer(uint16_t usage) {
  const uint8_t r[2] = {(uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8)};
  enqueue(kReportConsumer, r, sizeof r);
}

bool BleHid::connected() const { return running_ && g_connected; }
uint8_t BleHid::leds() const { return g_leds; }

}  // namespace t5m
