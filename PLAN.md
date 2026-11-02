# Tab5 Multitool — plan

An M5Stack Tab5, with or without the clip-on Tab5 Keyboard, acts as a keyboard
and/or mouse for another computer over **USB or Bluetooth**. Later it also
becomes a configurable macropad. Licence: **MIT**.

Two deliverables from one repo (`~/Developer/tab5-multitool`):

1. **The `Tab5Multitool` library.** Any Tab5 Arduino/PlatformIO sketch can
   include it and use only the parts it wants.
2. **The Tab5 Multitool firmware**, built on that library.

Order: finish the Multitool, then add it to T48 for Tab5. The T48 work doesn't
start until the library is done and published.

## Hardware facts this rests on

The ESP32-P4 has two USB OTG controllers, and the Tab5 wires them to different
sockets:

| Socket | P4 side | Used today for |
|---|---|---|
| USB-A | OTG **HS** controller + UTMI PHY (480 Mbps, proven by the T48 spike) | T48 / One ROM / gamepads (IDF `usb_host`) |
| USB-C | **FS** PHY, normally given to USB-Serial-JTAG | flashing + `Serial` console |

- **USB HID goes on USB-C, using the FS controller.** Arduino's own `USB.h` /
  `USBHIDKeyboard` can't be used here. On the P4 the core hard-codes TinyUSB
  device to rhport 1 (the HS controller), in `esp32-hal-tinyusb.c`. The library
  brings TinyUSB device up itself on rhport 0 with
  `usb_new_phy({controller OTG, target INT, mode DEVICE, speed FULL})`.
  Full speed is plenty for HID.
- **While USB HID is on, USB-Serial-JTAG is gone.** That means no `Serial` and
  no esptool auto-reset. The ROM goes back to serial-JTAG on reset, so download
  mode (hold the power button) still flashes. For debugging, the firmware gets a
  "no USB" mode (Bluetooth only, or local only) that leaves `Serial` alone.
- **Bluetooth goes through the C6.** The P4 has no radio. The Arduino core
  (pioarduino 55.03.31x) builds NimBLE over esp-hosted
  (`CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE=y`), and the BLE library has
  `BLEHIDDevice` and `Server_Gamepad`. So BLE HID is in reach, if the C6 slave
  firmware on the Tab5 has Bluetooth enabled.
- **Power back-feed.** `USB5V_EN` (the USB-A rail) back-drives USB-C VBUS. The
  Multitool doesn't use USB-A, so it keeps that rail **off**, and the problem
  doesn't come up.

## Phase 0 — spikes (results go in NOTES.md)

- **S1 USB device on FS.** A bare sketch: TinyUSB device on rhport 0, a boot
  keyboard that types "hello" every 5 s. Try the prebuilt `arduino_tinyusb` first
  (its config has `CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE`). Vendor TinyUSB source
  only if the prebuilt one won't take rhport 0. Done when a Mac and a PC
  enumerate it and it types. Also check it in a BIOS/UEFI screen and through a
  KVM, which only speak boot protocol.
- **S2 BLE HID through the C6.** `BLEHIDDevice` keyboard + mouse. Done when it
  pairs with a Mac and an iPhone/iPad, types, moves the pointer, and reconnects
  after a reboot (bond kept in NVS). If the C6 firmware has no BT, find out
  whether the esp-hosted slave firmware can be updated the way BarSense Hub's
  C6 was (2026-10-24).
- **S3 Real key state from the Tab5 Keyboard.** HID mode (reg 0x30) queues a
  press then a release. Does the release come on physical key-up? Hold a key
  and watch `k`. If yes, host auto-repeat and shift-click work. If no, try the
  keyboard's other modes (reg 0x10) or fall back to tap-only with our own repeat.
- **S4 Multi-touch.** Confirm `M5.Touch` gives two points steadily enough for
  two-finger scroll and two-finger tap = right-click.

## Library design (`Tab5Multitool`)

Layers, each usable alone. The key idea: **every input source emits the same
key event** `{usage, modifiers, down}`. Each event then goes either to a HID
transport (the Multitool) or to a "to text" helper for local text entry (T48).

1. **Input sources**
   - `Tab5Keyboard`: the clip-on keyboard reader, taken from T48's
     `keyboard.cpp` (G0/G1, INT G50, 0x6D, hot-plug probing).
   - `Keypad`: on-screen keyboards. A layout table goes in, and hit-tested
     key events come out. Built-in layouts: full QWERTY (+ shift/symbol pages),
     numpad (Num Lock follows the host LED), F-keys/nav cluster, media row.
     Keys fire on finger-down/up inside a key, never on a slide across it.
   - `Trackpad`: touch points go in, mouse actions come out. 1-finger move with
     acceleration, tap = left click, 2-finger tap = right click, 2-finger drag =
     scroll, tap-hold-drag = drag, optional on-screen L/R buttons.
   - `KeyText`: turns key events into characters / named keys (Enter, Esc,
     arrows…), on the US layout. T48 uses this for text entry.
2. **Transports**, behind one `HidSink` interface: `press/release(usage)`,
   `releaseAll()`, `typeText("…")` (paced), `mouseMove/buttons/scroll`,
   `consumer(key)`, `ledState()`, `connected()`.
   - `UsbHid`: owns the FS PHY + TinyUSB. Interfaces: boot keyboard (6KRO),
     mouse (buttons, X/Y, wheel, h-wheel), consumer control. `begin(name, vid,
     pid)` / `end()` (hands the PHY back to serial-JTAG), suspend and remote
     wakeup.
   - `BleHid`: NimBLE over esp-hosted, the same report map, pairing/bonding,
     battery level reporting.
   - `HidFanout`: sends to both at once, or to whichever is connected.
3. **Rendering.** The widgets don't draw. A default **M5GFX** renderer ships
   with them. T48 draws with M5GFX, and an LVGL user can write their own.
   Each widget takes a rectangle, so "full screen" just means a bigger rectangle.

Example use:

```cpp
#include <Tab5Multitool.h>
t5m::UsbHid usb;
t5m::Trackpad pad({0, 0, 1280, 720}, usb);   // full-screen mouse
void setup() { M5.begin(); usb.begin("Tab5 Multitool"); pad.draw(M5.Display); }
void loop()  { M5.update(); pad.handle(M5.Touch); }
```

Repo layout: `library.json` + `library.properties`, `src/`,
`examples/{Typewriter,Trackpad,Passthrough,BleKeyboard,LocalTextEntry}`, and the
firmware in `firmware/`.

## Multitool firmware

**Connection:** USB, Bluetooth, or both (setting). The status bar shows each
link's state, Caps/Num lock from the host, battery, and the current mode.

**On-screen only (no clip-on keyboard), three modes:**

| Mode | Screen |
|---|---|
| Keyboard | Full-screen on-screen keyboard (bigger keys; QWERTY / symbols / F-keys / numpad pages) |
| Keyboard + mouse | Keyboard in the lower part, trackpad above |
| Mouse | Full-screen trackpad (optional L/R/middle button strip) |

**Clip-on keyboard attached:** the physical keys go to the host. The screen
defaults to trackpad + numpad/nav/media strip. "Mouse" (full-screen trackpad)
and "numpad" are also available.

- **Switching modes:** a small, always-present mode button in the status bar.
  On the keyboard there's also a chord we pick once S3 shows which keys report.
  Hot-plugging the keyboard changes the layout to match, and unplugging it goes
  back to the last on-screen mode.
- **Settings** (NVS, written from `loop()` only): connection, pointer speed,
  natural scroll, tap-to-click, key click sound, brightness, idle dim, BLE
  device name, forget pairings.
- Web installer on GitHub Pages, like FlapBoard and T48.

## Final phase — macropad

- Pages of configurable buttons (grid size per page). Each button sends a key
  combo, a text string, a media key, or a short sequence with delays.
  Buttons have a label, colour, and optionally an icon.
- Layouts stored as JSON on LittleFS/SD. They're edited from a **web page served by
  the Tab5** (same pattern as T48's file manager) and can be exported and imported.
- Works over USB or BLE. A macropad page can also sit beside the trackpad.
- Library side: a `MacroPad` widget and a JSON loader, so other sketches can
  use it.

## Apps: launcher + Terminal (added 2026-11-01, before publishing)

The Multitool becomes a set of apps chosen from a home screen. The first two:

- **Keyboard & Mouse** - everything built so far (the five modes, Setup).
- **Terminal** - a VT100/xterm terminal built on Miguel de Icaza's
  [SwiftTerm](https://github.com/migueldeicaza/SwiftTerm) (MIT), connected over
  **SSH** (Wi-Fi) or a **USB RS-232 adapter** in the USB-A port.

### Launcher

- Home screen: one big tile per app, plus Setup. It opens on the last app used,
  so a keyboard-only user never sees it.
- Getting back home: a Home button at the left of each app's status bar, and
  Sym+Esc on the clip-on keyboard (already `kAppKey`).
- Each app is a small class: `begin()`, `end()`, `loop()`, `draw()`. Switching
  apps calls `end()` then `begin()`, so the terminal's Wi-Fi/SSH and USB host
  don't run while you're a keyboard, and HID doesn't run while you're a terminal.
  This is the same shape T48 needs for its "USB keyboard" mode (`runMultitool`).

### Terminal: what SwiftTerm gives us

SwiftTerm already has a Foundation-free headless core for Embedded Swift (the
`Embedded` package trait, Docs/embedded-swift.md): parser, screen buffers,
character sets, colours, keyboard encoding, scrollback. Not included: views,
PTYs, graphics. That is exactly the part we need - we draw, and we supply the
byte stream.

Build:
- Embedded Swift needs a Swift.org toolchain (6.3+). Xcode's Swift 6.4 here has
  no Embedded standard library for RISC-V (checked: "unable to load standard
  library for target riscv32-none-none-eabi").
- The P4 is RV32IMAFC, ilp32f ABI. Compile SwiftTerm + a thin Swift shim to a
  static library (`-target riscv32-none-none-eabi -march=rv32imafc_zicsr_zifencei
  -mabi=ilp32f`, release/WMO) with a script, and link the `.a` from PlatformIO -
  no change to the Arduino build system.
- The shim exports a small C API (`@_cdecl`): create(cols, rows), feed(bytes),
  key(usage, mods) -> bytes to send, read a row of cells (char + fg/bg/attrs),
  dirty-row range, cursor, title, resize. C++ does all drawing and I/O.

Screen:
- M5GFX, monospace bitmap font, redrawing dirty rows only. 1280x720 with a
  16x30 cell gives 80x23 under the status bar; a smaller font gives 106x30 or
  more. Font size is a setting.
- Clip-on keyboard: keys go to SwiftTerm's key encoder (Ctrl, Alt/Meta, arrows,
  F-keys on the Sym layer). No clip-on keyboard: the on-screen Qwerty keypad
  takes the bottom part of the screen, with a row for Esc, Ctrl, Tab and arrows.
- Touch: drag to scroll back.

SSH (Wi-Fi):
- libssh2 (BSD-3) on the mbedTLS that the Arduino core already ships. Vendored
  as source in the firmware.
- Saved hosts (name, host, port, user). Auth by password (typed at connect time,
  not stored) or by a key generated on the Tab5 (ECDSA; see Risks), whose public key the
  web page shows so you can paste it into `authorized_keys`.
- Host keys are remembered on first connect and a changed key is refused, with a
  "forget this host" button.

Serial (USB-A):
- IDF USB host (already in the Arduino core; T48 uses it) plus Espressif's
  `usb_host_cdc_acm` and `usb_host_vcp` class drivers (Apache-2.0, vendored):
  FTDI, CP210x, CH34x and CDC-ACM adapters.
- Baud rate, data bits, parity, stop bits, flow control; hot-plug.
- Turns on the USB-A port's 5 V while the app runs.

### Steps (status 2026-11-01: T1-T4 and T6 done on hardware (SSH with the
device key; password login untested); T5 written but not tried with an adapter)

T1. Launcher with the existing app as its only tile; Home button; app switching.
T2. Toolchain: install a Swift.org toolchain (swiftly), build a hello-world
    Embedded Swift `.a` for the P4, link it, call it from `setup()`.
T3. SwiftTerm Embedded core into that `.a` + the C shim; host test on the Mac
    feeding vttest-style output and dumping the screen.
T4. Terminal screen on the Tab5 fed by a local echo/loopback; clip-on and
    on-screen keyboard; colours; scrollback.
T5. USB serial (FTDI/CP210x/CH34x/CDC-ACM) with line settings.
T6. SSH with libssh2: password, then device key; saved hosts; host-key check.
T7. Docs + screenshots, then on to publishing (milestone 7).

### Risks

- Embedded Swift on the P4 is new ground: allocator hooks, atomics, FPU ABI
  matching the Arduino libs. T2 answers this before anything else depends on it.
- SwiftTerm's Embedded core size and speed on a 400 MHz core: measure in T3/T4.
- libssh2 against the core's mbedTLS 3.x build options (ed25519 needs care -
  mbedTLS has no ed25519, so libssh2 would use RSA/ECDSA keys from mbedTLS, or
  we add a small ed25519 implementation).

## T48 for Tab5 integration (after the Multitool is done)

Two separate things:

1. **On-screen keyboard for T48 itself (no HID needed).** T48 picks up
   `Tab5Keyboard` + `Keypad` + `KeyText` from the library. Chip search and other
   text fields then work without the clip-on keyboard, so you no longer scroll a
   long list to pick a chip. The clip-on keyboard reader is the same code it
   uses today, so nothing changes when the keyboard is attached.
2. **"USB keyboard" mode as its own mode.** It doesn't run while burning.
   Entering it stops T48 activity and powers off the USB-A port. Then it runs the
   Multitool's keyboard/mouse screens (USB and/or BLE). Leaving it calls `end()`,
   which restores `Serial` for `tools/mp.py`, and turns USB-A back on.
   The library supports this as one call, like `t5m::runMultitool(exitCallback)`,
   so any app can offer the whole thing as one menu entry.

## Status (2026-11-01)

Milestones 1–6 and 8 are done and run on hardware: USB + BLE, keyboard, on-screen modes, trackpad,
LEDs, macropad + web editor. Left: the app launcher + Terminal (see "Apps" above), then 7 (publish the library), the web installer, and 9 (T48).

## Milestones

1. Phase 0 spikes S1–S4.
2. `UsbHid` + `Typewriter` example (types a string, Caps LED readback).
3. `Tab5Keyboard` + `KeyText` + passthrough.
4. `Keypad` + `Trackpad` + M5GFX renderer + examples.
5. Firmware v0.1: USB, the three on-screen modes, the keyboard-attached layout,
   settings. Web installer.
6. `BleHid` + `HidFanout` → firmware v0.2 (Bluetooth).
7. Publish the library as v1.0 (GitHub `djb-rh/tab5-multitool`, PlatformIO
   registry; Arduino Library Manager later).
8. Macropad: widget, JSON format, web editor → firmware v1.x.
9. Then T48: shared keyboard + on-screen text entry first, then the USB keyboard
   mode → T48 v1.4.

## Decided

- Licence MIT. Project name **Tab5 Multitool**.
- T48: keyboard mode is separate from burning (not concurrent). T48 also gets
  the on-screen keyboard for its own text entry.
- Bluetooth keyboard/mouse is in scope. Macropad is the final phase.

## Still open

- VID/PID: Espressif's test PID for development; pid.codes for release, or let
  library users pass their own.
- Mode-switch chord on the clip-on keyboard (needs S3).
- BLE: whether "both at once" is worth it, or USB-or-BLE is enough.
