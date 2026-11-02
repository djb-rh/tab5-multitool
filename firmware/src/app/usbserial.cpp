#include "usbserial.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/stream_buffer.h>
#include <usb/usb_helpers.h>
#include <usb/usb_host.h>

#include <cstring>

namespace mt {
namespace usbserial {
namespace {

enum class Kind : uint8_t { None, CdcAcm, Ftdi, Pl2303, Cp210x, Ch34x };

struct State {
  usb_host_client_handle_t client = nullptr;
  usb_device_handle_t dev = nullptr;
  Kind kind = Kind::None;
  const char *name = nullptr;
  uint8_t intfs[2] = {0xFF, 0xFF};  // claimed interfaces
  uint8_t ctrlIntf = 0;             // wIndex for class/vendor requests
  uint8_t epIn = 0, epOut = 0;
  uint16_t mpsIn = 64, mpsOut = 64;
  uint16_t pid = 0;
  uint8_t ftdiPort = 0;             // FTDI wIndex: 0 single-port, 1.. per interface
  bool ftdiH = false;               // FT232H/FT2232H/FT4232H divisor
  uint16_t chVersion = 0;
  bool ready = false;
} g;

Line g_line;
volatile bool g_lineDirty = false;
volatile uint8_t g_pendingAddr = 0;
volatile bool g_gone = false;
volatile uint32_t g_generation = 0;
const char *g_problem = "";
bool g_registered = false;

StreamBufferHandle_t g_rx = nullptr, g_tx = nullptr;
SemaphoreHandle_t g_ctrlDone = nullptr, g_outDone = nullptr;
usb_transfer_t *g_ctrl = nullptr, *g_in = nullptr, *g_out = nullptr;
volatile bool g_inBusy = false;

// ------------------------------------------------------------- transfers --

void onCtrl(usb_transfer_t *) { xSemaphoreGive(g_ctrlDone); }
void onOut(usb_transfer_t *) { xSemaphoreGive(g_outDone); }

void submitIn();

void onIn(usb_transfer_t *t) {
  g_inBusy = false;
  if (t->status == USB_TRANSFER_STATUS_COMPLETED && t->actual_num_bytes > 0) {
    const uint8_t *p = t->data_buffer;
    int n = t->actual_num_bytes;
    if (g.kind == Kind::Ftdi) {
      // Every packet starts with two modem/line status bytes.
      for (int off = 0; off < n; off += g.mpsIn) {
        const int len = (n - off < g.mpsIn ? n - off : g.mpsIn) - 2;
        if (len > 0) xStreamBufferSend(g_rx, p + off + 2, len, 0);
      }
    } else {
      xStreamBufferSend(g_rx, p, n, 0);
    }
  }
  if (g.ready && t->status != USB_TRANSFER_STATUS_NO_DEVICE && t->status != USB_TRANSFER_STATUS_CANCELED) submitIn();
}

void submitIn() {
  if (!g.ready || g_inBusy) return;
  g_in->device_handle = g.dev;
  g_in->bEndpointAddress = g.epIn;
  g_in->callback = onIn;
  // FTDI: one packet per transfer keeps the status-byte stripping simple.
  g_in->num_bytes = g.kind == Kind::Ftdi ? g.mpsIn : usb_round_up_to_mps(512, g.mpsIn);
  g_in->timeout_ms = 0;
  g_inBusy = usb_host_transfer_submit(g_in) == ESP_OK;
}

// A control transfer on EP0, waiting for it. data is in or out per type.
bool control(uint8_t type, uint8_t req, uint16_t value, uint16_t index, uint8_t *data = nullptr, uint16_t len = 0) {
  usb_setup_packet_t *s = (usb_setup_packet_t *)g_ctrl->data_buffer;
  s->bmRequestType = type;
  s->bRequest = req;
  s->wValue = value;
  s->wIndex = index;
  s->wLength = len;
  if (!(type & 0x80) && len) memcpy(g_ctrl->data_buffer + 8, data, len);
  g_ctrl->num_bytes = 8 + len;
  g_ctrl->device_handle = g.dev;
  g_ctrl->bEndpointAddress = 0;
  g_ctrl->callback = onCtrl;
  g_ctrl->timeout_ms = 1000;
  xSemaphoreTake(g_ctrlDone, 0);
  if (usb_host_transfer_submit_control(g.client, g_ctrl) != ESP_OK) return false;
  if (xSemaphoreTake(g_ctrlDone, pdMS_TO_TICKS(1500)) != pdTRUE) return false;
  if (g_ctrl->status != USB_TRANSFER_STATUS_COMPLETED) return false;
  if ((type & 0x80) && len) memcpy(data, g_ctrl->data_buffer + 8, len);
  return true;
}

// ------------------------------------------------------- per-chip set-up --

bool cdcLine(uint8_t intf) {
  uint8_t lc[7];
  const uint32_t b = g_line.baud;
  lc[0] = b; lc[1] = b >> 8; lc[2] = b >> 16; lc[3] = b >> 24;
  lc[4] = g_line.stopBits;
  lc[5] = g_line.parity;
  lc[6] = g_line.dataBits;
  return control(0x21, 0x20, 0, intf, lc, 7) &&  // SET_LINE_CODING
         control(0x21, 0x22, 0x0003, intf);      // SET_CONTROL_LINE_STATE: DTR + RTS
}

uint32_t ftdiDivisor(uint32_t baud) {
  static const uint8_t frac[8] = {0, 3, 2, 4, 1, 5, 6, 7};
  if (g.ftdiH && baud >= 1200) {
    const uint32_t d3 = (8u * 120000000u / 10 + baud / 2) / baud;
    return ((d3 >> 3) | (uint32_t)frac[d3 & 7] << 14) | 0x20000;  // 120 MHz clock
  }
  const uint32_t d3 = (48000000u / 2 + baud / 2) / baud;  // in eighths
  uint32_t d = (d3 >> 3) | (uint32_t)frac[d3 & 7] << 14;
  if (d == 1) d = 0;
  else if (d == 0x4001) d = 1;
  return d;
}

bool ftdiApply() {
  const uint32_t div = ftdiDivisor(g_line.baud);
  uint16_t index = div >> 16;
  if (g.ftdiH || g.ftdiPort) index = (uint16_t)(index << 8 | g.ftdiPort);
  const uint16_t data = g_line.dataBits | g_line.parity << 8 | g_line.stopBits << 11;
  return control(0x40, 0x03, div & 0xFFFF, index) &&                          // SET_BAUDRATE
         control(0x40, 0x04, data, g.ftdiPort) &&                             // SET_DATA
         control(0x40, 0x02, 0, (g_line.rtscts ? 0x0100 : 0) | g.ftdiPort) && // SET_FLOW_CTRL
         control(0x40, 0x01, 0x0303, g.ftdiPort);                             // DTR + RTS
}

bool ftdiInit() {
  return control(0x40, 0x00, 0, g.ftdiPort) &&       // SIO_RESET
         control(0x40, 0x09, 2, g.ftdiPort) &&       // latency timer 2 ms: typing feels live
         ftdiApply();
}

bool plRead(uint16_t value) {
  uint8_t b;
  return control(0xC0, 0x01, value, 0, &b, 1);
}
bool plWrite(uint16_t value, uint16_t index) { return control(0x40, 0x01, value, index); }

bool pl2303Init() {
  // The vendor handshake from the Linux driver (HX / older types).
  plRead(0x8484); plWrite(0x0404, 0); plRead(0x8484); plRead(0x8383);
  plRead(0x8484); plWrite(0x0404, 1); plRead(0x8484); plRead(0x8383);
  plWrite(0, 1); plWrite(1, 0);
  plWrite(2, 0x44);
  return cdcLine(0) && plWrite(0, 0);  // flow control off
}

bool cp210xApply() {
  const uint8_t i = g.ctrlIntf;
  uint8_t baud[4] = {(uint8_t)g_line.baud, (uint8_t)(g_line.baud >> 8), (uint8_t)(g_line.baud >> 16),
                     (uint8_t)(g_line.baud >> 24)};
  const uint16_t lc = g_line.stopBits | g_line.parity << 4 | g_line.dataBits << 8;
  uint8_t flow[16] = {};
  if (g_line.rtscts) {
    flow[0] = 0x09;   // ulControlHandshake: DTR on, CTS handshake
    flow[4] = 0x80;   // ulFlowReplace: RTS handshake
    flow[8] = 128;    // ulXonLimit
    flow[12] = 128;   // ulXoffLimit
  } else {
    flow[0] = 0x01;
    flow[4] = 0x40;   // RTS on
  }
  return control(0x41, 0x1E, 0, i, baud, 4) &&  // SET_BAUDRATE
         control(0x41, 0x03, lc, i) &&          // SET_LINE_CTL
         control(0x41, 0x13, 0, i, flow, 16) && // SET_FLOW
         control(0x41, 0x07, 0x0303, i);        // SET_MHS: DTR + RTS
}

bool cp210xInit() { return control(0x41, 0x00, 1, g.ctrlIntf) && cp210xApply(); }  // IFC_ENABLE

uint16_t ch34xDivisor(uint32_t speed) {
  constexpr uint32_t kClk = 48000000;
  auto clkDiv = [](int ps, int fact) { return 1u << (12 - 3 * ps - fact); };
  if (speed < 47) speed = 47;
  if (speed > 2000000) speed = 2000000;
  int fact = 1, ps;
  for (ps = 3; ps >= 0; ps--)
    if (speed > kClk / (clkDiv(ps, 1) * 512)) break;
  if (ps < 0) ps = 0;
  uint32_t cd = clkDiv(ps, fact);
  uint32_t div = kClk / (cd * speed);
  if (div < 9 || div > 255) {
    div /= 2;
    cd *= 2;
    fact = 0;
  }
  if (div < 2) div = 2;
  if (16 * kClk / (cd * div) - 16 * speed >= 16 * speed - 16 * kClk / (cd * (div + 1))) div++;
  if (fact == 1 && div % 2 == 0) {
    div /= 2;
    fact = 0;
  }
  return (uint16_t)((0x100 - div) << 8 | fact << 2 | ps);
}

bool ch34xApply() {
  uint16_t val = ch34xDivisor(g_line.baud);
  if (g.chVersion > 0x27) val |= 0x80;  // don't hold data back for full packets
  if (!control(0x40, 0x9A, 0x1312, val)) return false;
  uint8_t lcr = 0xC0 | (g_line.dataBits - 5);  // RX + TX enabled, word length
  if (g_line.stopBits == 2) lcr |= 0x04;
  switch (g_line.parity) {
    case 1: lcr |= 0x08; break;
    case 2: lcr |= 0x18; break;
    case 3: lcr |= 0x28; break;
    case 4: lcr |= 0x38; break;
  }
  if (g.chVersion >= 0x30 && !control(0x40, 0x9A, 0x2518, lcr)) return false;
  return control(0x40, 0xA4, (uint16_t)~0x60, 0);  // DTR + RTS (active low)
}

bool ch34xInit() {
  uint8_t ver[2] = {};
  if (!control(0xC0, 0x5F, 0, 0, ver, 2)) return false;
  g.chVersion = ver[0];
  return control(0x40, 0xA1, 0, 0) && ch34xApply();
}

bool applyLine() {
  switch (g.kind) {
    case Kind::CdcAcm: return cdcLine(g.ctrlIntf);
    case Kind::Pl2303: return cdcLine(0);
    case Kind::Ftdi: return ftdiApply();
    case Kind::Cp210x: return cp210xApply();
    case Kind::Ch34x: return ch34xApply();
    default: return false;
  }
}

// --------------------------------------------------------- open / close --

// The first bulk IN + OUT pair on an interface.
bool bulkPair(const usb_config_desc_t *cfg, const usb_intf_desc_t *intf) {
  g.epIn = g.epOut = 0;
  for (int i = 0; i < intf->bNumEndpoints; i++) {
    int off = 0;
    const usb_ep_desc_t *ep = usb_parse_endpoint_descriptor_by_index(intf, i, cfg->wTotalLength, &off);
    if (!ep || (ep->bmAttributes & 3) != 2) continue;  // bulk only
    if (ep->bEndpointAddress & 0x80) {
      if (!g.epIn) { g.epIn = ep->bEndpointAddress; g.mpsIn = ep->wMaxPacketSize & 0x7FF; }
    } else if (!g.epOut) {
      g.epOut = ep->bEndpointAddress;
      g.mpsOut = ep->wMaxPacketSize & 0x7FF;
    }
  }
  return g.epIn && g.epOut;
}

const char *ftdiName(uint16_t pid) {
  switch (pid) {
    case 0x6001: return "FTDI FT232R";
    case 0x6015: return "FTDI FT-X";
    case 0x6010: return "FTDI FT2232";
    case 0x6011: return "FTDI FT4232H";
    case 0x6014: return "FTDI FT232H";
    default: return "FTDI";
  }
}

void closeDevice() {
  if (!g.dev) return;
  g.ready = false;
  if (g_inBusy) usb_host_endpoint_halt(g.dev, g.epIn), usb_host_endpoint_flush(g.dev, g.epIn);
  for (int t = 0; t < 50 && g_inBusy; t++) delay(2);
  for (uint8_t i : g.intfs)
    if (i != 0xFF) usb_host_interface_release(g.client, g.dev, i);
  usb_host_device_close(g.client, g.dev);
  g = State{g.client};
  g_generation++;
}

void openDevice(uint8_t addr) {
  if (usb_host_device_open(g.client, addr, &g.dev) != ESP_OK) {
    g.dev = nullptr;
    return;
  }
  const usb_device_desc_t *dd;
  const usb_config_desc_t *cfg;
  usb_host_get_device_descriptor(g.dev, &dd);
  usb_host_get_active_config_descriptor(g.dev, &cfg);
  const uint16_t vid = dd->idVendor;
  g.pid = dd->idProduct;
  g_problem = "";

  int off = 0;
  const usb_intf_desc_t *dataIntf = nullptr, *commIntf = nullptr;
  if (vid == 0x0403) {
    g.kind = Kind::Ftdi;
    g.name = ftdiName(g.pid);
    g.ftdiH = g.pid == 0x6010 || g.pid == 0x6011 || g.pid == 0x6014;
    g.ftdiPort = (g.pid == 0x6010 || g.pid == 0x6011) ? 1 : 0;  // port A of a multi-port chip
    dataIntf = usb_parse_interface_descriptor(cfg, 0, 0, &off);
  } else if (vid == 0x067B) {
    g.kind = Kind::Pl2303;
    g.name = "Prolific PL2303";
    dataIntf = usb_parse_interface_descriptor(cfg, 0, 0, &off);
  } else if (vid == 0x10C4) {
    g.kind = Kind::Cp210x;
    g.name = "Silicon Labs CP210x";
    dataIntf = usb_parse_interface_descriptor(cfg, 0, 0, &off);
  } else if (vid == 0x1A86 && (g.pid == 0x7523 || g.pid == 0x5523 || g.pid == 0x7522)) {
    g.kind = Kind::Ch34x;
    g.name = g.pid == 0x5523 ? "WCH CH341" : "WCH CH340";
    dataIntf = usb_parse_interface_descriptor(cfg, 0, 0, &off);
  } else {
    // CDC-ACM: a communications interface (class 2) and a data interface (class 10).
    for (int i = 0; i < cfg->bNumInterfaces; i++) {
      off = 0;
      const usb_intf_desc_t *d = usb_parse_interface_descriptor(cfg, i, 0, &off);
      if (!d) continue;
      if (d->bInterfaceClass == 0x02 && d->bInterfaceSubClass == 0x02 && !commIntf) commIntf = d;
      if (d->bInterfaceClass == 0x0A && !dataIntf) dataIntf = d;
    }
    if (dataIntf) {
      g.kind = Kind::CdcAcm;
      g.name = "USB serial (CDC-ACM)";
    }
  }
  Serial.printf("usbserial: %04x:%04x %s\n", vid, g.pid, g.name ? g.name : "(not a serial adapter)");
  if (!dataIntf || !bulkPair(cfg, dataIntf)) {
    g_problem = g.kind == Kind::None ? "not a serial adapter this app knows" : "no bulk endpoints";
    usb_host_device_close(g.client, g.dev);
    g = State{g.client};
    g_generation++;
    return;
  }
  g.intfs[0] = dataIntf->bInterfaceNumber;
  g.ctrlIntf = commIntf ? commIntf->bInterfaceNumber : dataIntf->bInterfaceNumber;
  if (commIntf) g.intfs[1] = commIntf->bInterfaceNumber;
  for (uint8_t i : g.intfs) {
    if (i == 0xFF) continue;
    if (usb_host_interface_claim(g.client, g.dev, i, 0) != ESP_OK) {
      g_problem = "could not claim its interface";
      if (i == g.intfs[1]) g.intfs[1] = 0xFF;
      else { closeDevice(); return; }
    }
  }
  if (g.mpsIn > 512) g.mpsIn = 512;
  g.ready = true;  // control() needs the device; ready gates the IN pump
  bool ok;
  switch (g.kind) {
    case Kind::Ftdi: ok = ftdiInit(); break;
    case Kind::Pl2303: ok = pl2303Init(); break;
    case Kind::Cp210x: ok = cp210xInit(); break;
    case Kind::Ch34x: ok = ch34xInit(); break;
    default: ok = cdcLine(g.ctrlIntf); break;
  }
  if (!ok) {
    // CDC devices without line coding (some microcontroller boards) still work.
    Serial.printf("usbserial: %s did not take the line settings\n", g.name);
    if (g.kind != Kind::CdcAcm) g_problem = "the adapter refused the line settings";
  }
  xStreamBufferReset(g_rx);
  xStreamBufferReset(g_tx);
  submitIn();
  g_generation++;
}

// ----------------------------------------------------------------- tasks --

void onClientEvent(const usb_host_client_event_msg_t *msg, void *) {
  if (msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV) g_pendingAddr = msg->new_dev.address;
  else if (msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE) g_gone = true;
}

void hostTask(void *) {
  for (;;) {
    uint32_t flags = 0;
    usb_host_lib_handle_events(portMAX_DELAY, &flags);
    if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) usb_host_device_free_all();
  }
}

// Delivers transfer callbacks; never blocks on a transfer itself.
void clientTask(void *) {
  for (;;) usb_host_client_handle_events(g.client, pdMS_TO_TICKS(10));
}

// Opens and sets up adapters and sends queued bytes. Control transfers wait
// here while clientTask delivers their completion.
void workerTask(void *) {
  uint32_t lastPoll = 0;
  for (;;) {
    if (g_gone) {
      g_gone = false;
      g_problem = "";
      closeDevice();
    }
    // A device present before the client registered gets no NEW_DEV event.
    if (!g.dev && millis() - lastPoll > 1000) {
      lastPoll = millis();
      uint8_t addrs[4];
      int n = 0;
      if (usb_host_device_addr_list_fill(sizeof addrs, addrs, &n) == ESP_OK && n > 0 && !g_problem[0])
        g_pendingAddr = addrs[0];
    }
    if (g_pendingAddr && !g.dev) {
      const uint8_t a = g_pendingAddr;
      g_pendingAddr = 0;
      openDevice(a);
    }
    if (g.ready && g_lineDirty) {
      g_lineDirty = false;
      applyLine();
    }
    if (g.ready && xStreamBufferBytesAvailable(g_tx)) {
      const size_t n = xStreamBufferReceive(g_tx, g_out->data_buffer, 512, 0);
      g_out->num_bytes = n;
      g_out->device_handle = g.dev;
      g_out->bEndpointAddress = g.epOut;
      g_out->callback = onOut;
      g_out->timeout_ms = 0;
      xSemaphoreTake(g_outDone, 0);
      if (usb_host_transfer_submit(g_out) == ESP_OK && xSemaphoreTake(g_outDone, pdMS_TO_TICKS(2000)) != pdTRUE) {
        // Stuck (flow control held off?): give up on these bytes.
        usb_host_endpoint_halt(g.dev, g.epOut);
        usb_host_endpoint_flush(g.dev, g.epOut);
        xSemaphoreTake(g_outDone, pdMS_TO_TICKS(500));
        usb_host_endpoint_clear(g.dev, g.epOut);
      }
      continue;
    }
    if (g.ready && !g_inBusy) submitIn();
    delay(2);
  }
}

}  // namespace

volatile bool g_portOn = false;

void begin() {
  if (g_rx) {
    if (!g_portOn) {
      M5.Power.setExtOutput(true, m5::ext_USB);
      usb_host_lib_set_root_port_power(true);
      g_portOn = true;
    }
    return;
  }
  g_rx = xStreamBufferCreate(16384, 1);
  g_tx = xStreamBufferCreate(4096, 1);
  g_ctrlDone = xSemaphoreCreateBinary();
  g_outDone = xSemaphoreCreateBinary();
  usb_host_transfer_alloc(8 + 64, 0, &g_ctrl);
  usb_host_transfer_alloc(512, 0, &g_in);
  usb_host_transfer_alloc(512, 0, &g_out);

  M5.Power.setExtOutput(true, m5::ext_USB);  // 5 V on the USB-A port
  usb_host_config_t hc = {};
  hc.intr_flags = ESP_INTR_FLAG_LEVEL1;
  hc.root_port_unpowered = true;
  if (usb_host_install(&hc) != ESP_OK) {
    g_problem = "USB host failed to start";
    return;
  }
  usb_host_client_config_t cc = {};
  cc.is_synchronous = false;
  cc.max_num_event_msg = 5;
  cc.async.client_event_callback = onClientEvent;
  if (usb_host_client_register(&cc, &g.client) != ESP_OK) {
    g_problem = "USB host failed to start";
    return;
  }
  xTaskCreatePinnedToCore(hostTask, "usbhost", 4096, nullptr, 5, nullptr, 0);
  xTaskCreatePinnedToCore(clientTask, "usbclient", 4096, nullptr, 5, nullptr, 0);
  xTaskCreatePinnedToCore(workerTask, "usbserial", 6144, nullptr, 4, nullptr, 0);
  // Power the port only once the client is listening, or an adapter that is
  // already plugged in is announced to nobody.
  usb_host_lib_set_root_port_power(true);
  g_portOn = true;
}

void end() {
  if (!g_rx || !g_portOn) return;
  // Unpowering the port makes the adapter go away; the worker closes it.
  usb_host_lib_set_root_port_power(false);
  M5.Power.setExtOutput(false, m5::ext_USB);
  g_portOn = false;
}

void setLine(const Line &line) {
  g_line = line;
  g_lineDirty = true;
}
const Line &line() { return g_line; }
const char *adapter() { return g.ready ? g.name : nullptr; }
uint32_t generation() { return g_generation; }
const char *problem() { return g_problem; }

size_t read(uint8_t *buf, size_t cap) { return g_rx ? xStreamBufferReceive(g_rx, buf, cap, 0) : 0; }
size_t write(const uint8_t *data, size_t n) { return g.ready ? xStreamBufferSend(g_tx, data, n, 0) : 0; }

}  // namespace usbserial
}  // namespace mt
