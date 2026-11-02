# Tab5 Multitool — working notes

Newest first. PLAN.md is the plan; this is what was actually found.

## 2026-11-02 (unattended): T48, Calculator, Units

- **T48 + One ROM app.** T48 for Tab5 (~/Developer/tab5-burner, branch
  `launcher-app`, uncommitted) now builds as a library: its setup()/loop() moved
  to burner.cpp (burner::setup/loop; main.cpp there is a two-line wrapper),
  and host.h lets a launcher add a Home button and pass its Wi-Fi network
  (used only while the burner has none saved). Here it is lib/t48 over
  `third_party/t48-for-tab5`, a submodule tracking djb-rh/t48-for-tab5 branch
  launcher-app (edit in ~/Developer/tab5-burner, push, then bump the submodule).
  tools/build_t48.py builds its Rust part.
  - It starts from a clean boot (its own USB host, SD, Wi-Fi, web server,
    keyboard driver): setup() checks the remembered app before M5.begin and
    hands the whole boot to burner::setup(). Home restarts to the home screen.
    Two crashes at start and the boot goes home instead.
  - minipro's libb64 cdecode.c clashed with the Arduino core's at link time
    (an archive here, not loose objects as in the standalone build): its
    symbols are renamed with -D flags in lib/t48/library.json.
  - Checked: tile -> T48 (joins Wi-Fi via the launcher's network, web page
    answers, 265 KB DMA RAM free) -> Home. This Tab5 has no SD card and no
    T48 was attached: nothing programmed.
  - The firmware with T48 linked in is GPL-3.0-or-later (README says so).
- **Calculator** (calc_app.cpp over rpn.cpp; host test test/rpn_test.sh).
  The Japanese gothic font has × ÷ √ π Σ Δ Γ ↓ ↑ ← → but NOT − (U+2212), ↔,
  ², ˣ or ŷ: labels use ASCII for those. ASCII-only labels are drawn in
  DejaVu (the JP font's Latin looks off). Checked: sin 30 = 0.5, 12.5 ENTER 3
  × 2 √, divide by zero (error shown, stack kept), HEX entry, every page.
  State saved to NVS "calc" (rpn::State blob).
- **Units** (units_app.cpp over units.cpp; host test test/units_test.sh,
  24 kinds, 208 units). Checked: "5 ft 3 in to cm", "100 F in C" (switches
  the kind), "3/8 in mm" (shows = 3/8 in). NOT checked: the on-screen number
  pad (it only shows with the clip-on keyboard off, which was on).
- Console: from home `c` Calculator, `u` Units, `r` T48; in Calculator and
  Units `i<line>` types (newline = Enter) and `@x,y` taps. In T48 the console
  is the burner's (tools/tab5.py in that repo: `tap 55 28` is Home).
- Flash is 4.79 MB of the 6 MB slot.

## 2026-11-01 (later): Home without restarting

- Terminal -> Home no longer restarts: terminalEnd() ends SSH and powers USB-A
  off (usbserial::end); coming back keeps the screen, scrollback and prompt.
  Home is requested from inside an app's loop pass and acted on after it.
- Keyboard & Mouse still starts only from a clean boot (USB-C PHY swap, BLE):
  opening it after the Terminal ran restarts straight into it, and Home from
  it restarts. Checked on hardware: no reset for Terminal<->Home, one each
  way for Keyboard.
- `pio run -e release`: the app without the 3 s USB-Serial-JTAG wait at boot.
- Two Espressif boards on the Mac (a LogoWatch StopWatch showed up as
  303A:1001 too): flash.py now passes `--chip esp32p4`, and flash.py/shot.py
  stop and ask for T5M_PORT when several 303A devices are connected. This
  Tab5's USB serial number is its MAC.

## 2026-11-01 (late): SSH works

- libssh2 1.11.1 (third_party/libssh2 submodule, BSD-3) built as a PlatformIO
  lib (firmware/lib/libssh2/library.json points at the submodule; our
  config/libssh2_config.h). It compiled and linked against the core's mbedTLS
  3.6.6 first time. Kex/host keys: ECDH + ECDSA/RSA (no Curve25519/Ed25519 in
  libssh2's mbedTLS backend); OpenSSH servers are fine with that.
- Two libssh2 mbedTLS-backend snags for an ECDSA user key:
  1. It can only derive a public key from an RSA private key: pass the
     OpenSSH public key line to userauth_publickey_frommemory too.
  2. ecdsa_new_private_frommemory copies the PEM into a buffer one byte longer
     and leaves that byte unset; mbedTLS only takes PEM ending in a zero. A
     calloc-based allocator (libssh2_session_init_ex) fixes it.
- Tested against a throwaway key-only sshd on the Mac (port 2222, config in the
  session scratchpad, stopped after): key login, xterm-256color 106x27, ls -G
  colours, top, vim (alternate screen, colours, status lines), Enter ~ .
- The device key: ECDSA P-256 made on first run, PEM in NVS "terminal"/sshkey;
  public line at http://<tab5>/ssh-key.pub and Setup -> "SSH: this Tab5's key".
- Console: typed text must not reach other console commands (an `h` in
  `echo` sent the Tab5 home): the terminal's `i`/`f` line capture runs first.
- Password logins (password + keyboard-interactive) are written but untested.

## 2026-11-01 (night): app launcher, Embedded Swift, SwiftTerm, USB serial

Launcher:
- Home screen with app tiles; the last app opens at boot. One app per boot: Home
  sets an RTC flag and restarts, so apps never share USB-C, USB-A or the radio.
  T48 can join as another app the same way (it is GPL-3, so a firmware with it
  in is GPL; the library stays MIT).
- Console: `h` home, `k` / `t` open Keyboard / Terminal from home, `w` Swift
  self-test, `f<text>\n` (terminal) shows text as if received.

Embedded Swift on the P4 (swift/build.sh):
- Xcode's Swift has no RISC-V Embedded stdlib. The Swift.org toolchain does
  (installed with swiftly 2026-11-01, `--no-modify-profile`, so Xcode's swift
  stays the default; build.sh uses ~/Library/Developer/Toolchains/swift-latest).
- Flags: `-target riscv32-none-none-eabi -Xcc -march=rv32imafc_zicsr_zifencei
  -Xcc -mabi=ilp32f -Xcc -fno-pic` -> ELF flags 0x3 (RVC, single-float) like the
  Arduino libs.
- Three link problems, all handled in build.sh:
  1. libswiftUnicodeDataTables.a ships soft-float (e_flags 0x1). It is only
     data: rewrite e_flags to 0x3.
  2. Swift reaches `_swift_stdlib_nfd_decompositions` through the GOT even
     non-PIC (no flag fixes it), and IDF's sections.ld discards .got.
     fix_got.py turns the auipc/GOT_HI20 + lw pair into auipc/PCREL_HI20 + addi.
  3. `.swift_modhash` is an orphan section: objcopy --remove-section.
- Self-test (classes, Array, Dictionary<String,Int>, Unicode String, Float)
  passes on hardware; 0.2 ms warm.

SwiftTerm (third_party/SwiftTerm submodule, MIT):
- Its `Embedded` trait is a Foundation-free core: compile every source except
  Apple/Mac/iOS with `-D SWIFTTERM_EMBEDDED` (NOT the
  SWIFTTERM_EMBEDDED_NEEDS_SWIFT_6_3 define: that is an old-SwiftPM #error).
  The build-info plugin's two generated files are kept in swift/Generated/.
- ~30 s to compile; adds ~1.4 MB of flash. No terminal lock in Embedded:
  one thread per terminal.
- Our C interface: swift/include/t5m_term.h + Sources/T5MSwift/Term.swift,
  built on SwiftTerm's portable host API (makeRenderSnapshot, sendHostKey,
  sendHostText, scrollViewport). test/term_test.sh runs it on the Mac.
- sendHostKey returns 0 for plain printable keys (text belongs to
  sendHostText); with Ctrl/Alt it encodes them.
- SwiftTerm's default text colour is a dim grey; we set our own fg/bg.

Terminal screen:
- lgfxJapanGothic_24 (12x24 cells, 106x27 with the clip-on keyboard).
  U8g2-style fonts: drawChar's y is the baseline, not the top.
- Box drawing (U+2500-257F) and blocks (U+2580-259F) are drawn as lines and
  fills: the font's glyphs are full-width and don't join.
- Each row is drawn into a 1280x24 PSRAM sprite and pushed, so no flicker.

USB serial (firmware/src/app/usbserial.cpp):
- IDF USB host on USB-A, our own drivers following Linux: CDC-ACM, FTDI
  (BM and H divisors, 2 status bytes per packet), PL2303 (HX vendor
  handshake), CP210x, CH340/CH341 (divisor + LCR). NOT tested with an adapter yet.
- Control transfers wait in a worker task while the client task delivers
  completions (waiting inside the client task would deadlock).

## 2026-11-01 (evening): macropad flashing

- Every tap blanked and redrew the whole macropad (press, slide and release
  each did it), which showed as flashes. MacroPadRenderer now redraws the
  whole pad only on a page switch or new layout. Otherwise it redraws just
  the button or tab whose pressed state changed, like KeypadRenderer does.
  Tabs now shade while pressed too.
- The example's first page was "Edit" (edit commands), which reads as "edit
  the macros"; renamed it "Shortcuts". The editor is the web page.

## 2026-11-01 (afternoon, unattended): macropad

- Library: `Macro.h/.cpp` (layout JSON → pages/buttons/steps; `parseCombo`, `parseStep`;
  `MacroRunner` plays steps without blocking, delays included), `MacroPad` (grid + page tabs; fires
  on release over the same button), `MacroPadRenderer`. ArduinoJson 7 is now a library dependency.
- Firmware: fifth mode **Macros**; layout in LittleFS `/macros.json` (the example shows when
  there's none); web editor (`web.cpp`, `web_page.h`) at http://<ip>/ and
  http://tab5-multitool.local/. API: `GET/POST /api/layout`, `GET /api/example`, `POST /api/reset`.
  Setup has a "Macro web editor" row (on/off, shows the URL).
- Console: `1`-`5` pick a mode, `>` next macro page, `S` Setup, `P` screenshot (`tools/shot.py`).
- **Colour bug found by screenshot:** LovyanGFX takes a plain `int` colour literal as RGB565, so
  `0x7CFC9A` (indicator green) was pink on the screen and the bar was navy. Literals are now
  `constexpr uint32_t`.
- Verified without touching any macro (the Mac is paired over BLE): host tests `test/run.sh`
  all pass; screenshots of all three example pages and Setup (docs/shots/); curl: page 200 (14 KB,
  ~0.1 s warm), layout GET, bad combo → 400 with "page "T", button 1 ("X"): unknown key "banana"",
  save → 200 and read back, reset → example; editor JS passes `node --check` and its
  load/save round trip (incl. `\n` in text steps); mDNS resolves (curl needs `-4`, or it waits on IPv6).
- NOT verified: the editor in a real browser, and any macro actually firing on the Mac.
- Left the Tab5 in Mouse mode, Bluetooth link, as Donnie had it.

## 2026-11-01 — first session (unattended part)

### Device and toolchain
- Tab5 chip rev v1.3. **ST7121 display / ST7123 touch** (the newer panel;
  M5GFX from git handles it). It came with M5's factory demo (V0.4, IDF 5.5.3).
  Full 16 MB backup: `backup/tab5-factory-20261101.bin` (gitignored). Restore with
  `esptool --port <port> write-flash 0 backup/tab5-factory-20261101.bin`.
- Toolchain: PlatformIO 6.2.0 (newest), pioarduino **55.03.312-1** = Arduino 3.3.12 / IDF 5.5.5,
  the newest stable (61.04.00 / IDF 6.1 is still RC). Same pin as T48.
- **C6 esp-hosted updated 1.4.1 → 2.12.13** (probe `u`, from Espressif's server). Versions match now.
  After the update Wi-Fi sat at status 0 (idle) until `WIFI_ALL_CHANNEL_SCAN` +
  `WIFI_CONNECT_AP_BY_SIGNAL` were set; then it joins at once (-55 dBm).
- **Tab5 Keyboard firmware 1** = the newest M5 has published (M5Tab5-Keyboard-Internal-FW,
  `FIRMWARE_VERSION 0x01`, hex dated 2026-05-06). Nothing to update.
- 16 MB partition table (`firmware/partitions.csv`): two 6 MB app slots + 3.9 MB LittleFS.
  The default 1.25 MB slot is too small even for the probe.

### Tab5 Keyboard (S3 answered from M5's source, MIT)
- Register map: 0x00 int cfg, 0x02 event count, 0x03 RGB brightness (0–100, scales the
  colours), 0x10 key mode (0 key / 1 HID / 2 char), 0x11 RGB mode (1 = user colours),
  0x20 key event, 0x30 HID event, 0x40/0x50 char events, 0x60–0x67 RGB (B,G,R,pad × 2 LEDs),
  0xFD IAP, 0xFE firmware, 0xFF I2C address.
- **Key mode (0)**: one byte per event, bit7 pressed, bits 6–4 row, 3–0 col, sent on real
  key-down AND key-up. That gives true held state (host auto-repeat, chords). HID mode only
  sends "usage 0" on release, i.e. one key at a time. The library uses key mode and its own
  legend table copied from M5's firmware.
- Two WS2812 LEDs, individually settable. Library default brightness 6 % (colours at 255).
- Not checked on hardware yet: typing (needs a person), and how dim 6 % looks.

### USB HID on USB-C (S1)
- Arduino's `USB.h` can't be used: on the P4 it hard-codes TinyUSB device mode to rhport 1
  (HS/UTMI = the USB-A socket). The prebuilt `libarduino_tinyusb.a` knows rhport 0 = FS OTG
  (0x50040000), so `tusb_init(0, {DEVICE, FULL})` works with no vendoring. The core's own
  `esp32-hal-tinyusb.o` is not pulled in, so our descriptor callbacks don't clash.
- Prebuilt config: `CFG_TUD_HID = 1` (one HID interface), `CFG_TUD_CDC = 2`. So keyboard +
  mouse + consumer share one boot-keyboard interface under report IDs 1/2/3, and boot protocol
  sends the keyboard report without its ID.
- **The P4 has two FSLS PHYs.** USB-Serial-JTAG is on PHY 0 (wired to the Tab5's USB-C), and
  the FS OTG controller defaults to PHY 1. `LP_SYS.usb_ctrl.sw_hw_usb_phy_sel = 1;
  sw_usb_phy_sel = 1` swaps them. **LP_SYS survives esp_restart()**, so the library undoes the
  swap in a shutdown handler AND in a static constructor at boot. The first spike didn't, and that
  is why the Tab5 dropped off the bus (see below).
- Result: the Mac enumerated "Tab5 Multitool" 303A:82F0 at 12 Mbps with the right descriptors
  (signature shows HID 03/01/01 + CDC 02/02/00 + 0A). The device stayed
  `!registered, !matched`: **the Mac's screen was locked**. macOS on Apple Silicon makes you
  approve a new USB accessory ("Allow accessory to connect?") while unlocked. **NEEDS DONNIE.**
- The spike's 60-second "no console → back to JTAG" fallback didn't bring the port back.
  Likely cause: the PHY swap surviving the restart (fixed in the library since). The Tab5 needs a
  power cycle (double-press power = off, press = on), or hold power for download mode.

### What exists now (all builds; not yet run on the Tab5)
- Library `src/t5m/`: UsbHid (+ CDC console), BleHid (NimBLE over esp-hosted), HidFanout,
  Tab5Keyboard (key mode, Sym layer, Aa = Shift / double-tap Caps, Sym+Alt = GUI,
  Sym+Esc = app key, LEDs), Keypad (QWERTY / Fn+symbols / numpad / nav+media pages,
  latch/lock modifiers), Trackpad (move+accel, tap / 2-finger right-click / 3-finger
  middle, 2-finger scroll, tap-drag, L/M/R strip), KeyText, M5GFX renderer.
- Firmware `firmware/src/app`: the four modes, status bar, settings screen, keyboard LEDs
  (left Wi-Fi, right Bluetooth or USB), console commands.
- `tools/flash.py`: flashes whatever state the Tab5 is in (asks the CDC console for JTAG
  first; `--wait` waits for a power cycle).

- Dev builds of the app (`-DT5M_BOOT_JTAG_MS=3000`) stay on USB-Serial-JTAG for 3 s at
  every boot, then start USB HID. `tools/flash.py` builds, waits for the JTAG port, and writes
  the app with esptool at once, so it catches that window. The `j` console command and the
  "Firmware update mode" setting restart into JTAG for one boot.

### Later the same day: app running
- The Tab5 reappeared on USB-Serial-JTAG and `flash.py --wait --full` wrote the app.
- After the 3 s window it re-enumerated as "Tab5 Multitool": `registered, matched`, macOS
  attached `AppleUserHIDEventDriver`, and the CDC console came up as
  `/dev/cu.usbmodem<MAC>2` (named after the MAC).
- Console `s`: mode Pad+Num, link USB, keyboard present (fw 1), usb mounted 1 / suspended 0,
  Wi-Fi up, battery 100%, heap 403 KB, PSRAM 29.8 MB free.
- **First keys verified on the Mac:** macOS's Keyboard Setup Assistant came up (unknown keyboard).
  Donnie pressed Z on the clip-on keyboard (right of left Shift) and Sym+* for '/' (left of right Shift).
  It identified the keyboard as **ANSI**. So the clip-on keyboard → key mode → USB HID path works,
  Sym layer included.
- Still unverified: on-screen keys, the pointer, Caps/Num LED feedback, BLE.

### Round 2 (Donnie's first test results)
- Works: typing + holding (auto-repeat), trackpad incl. gestures, on-screen keys.
- **Caps Lock double-tap did nothing.** It sent press+release ~1 ms apart, and macOS ignores Caps Lock
  taps that short. Both transports now hold a Caps Lock release until it has been down 250 ms
  (`kCapsMinHoldMs`), whatever produced it. Double-tap window 350 → 400 ms.
- On-screen legends follow Shift / Caps Lock (Keypad::labelFor; the renderer redraws changed labels).
- Numpad "Num" relabelled "Num/Clr": HID Num Lock is Clear on a Mac (macOS keypads ignore Num Lock).
- LED default 6 % → 3 %; settings version 2 resets an older saved value to the new default.
- `flash.py` round trip proven hands-off: console `j` → JTAG → esptool. Afterwards the P4 stayed in
  reset until the port was reopened, so flash.py now opens it once with RTS/DTR low.

- **Verified by Donnie:** Caps Lock double-tap works with the 250 ms hold.

### Round 3: Bluetooth boot loop
- Switching to Bluetooth gave a panic loop: Load access fault in `BLECharacteristic::setValue`
  from `BleHid::begin`. The core's `BLEHIDDevice::manufacturer(String)` writes
  `m_manufacturerCharacteristic`, which only the getter `manufacturer()` creates, so on its own it
  dereferences null. Fixed with `manufacturer()->setValue(...)`, as the core's example does.
- Guard added: two panic/watchdog resets in a row before 15 s of uptime, and the next boot runs USB
  only (no BLE) with a note on screen. A transport that crashes at start can't strand the Tab5.
- After the fix: `ble: running 1 connected 0` (advertising), and Wi-Fi joins alongside it
  (up by 30 s). Pairing not tried yet.

### Round 4: Bluetooth pairing (four separate problems)
1. **Stale key on the Mac.** Early builds bonded with no key distribution (`BLESecurity`'s static
   setters were used without constructing one, so its constructor never set init/resp keys). The
   Mac kept a half-bond and tried to encrypt with it on connect (`TS_LE_EncryptionCompleteCb
   STATUS 4803`). macOS can't forget a device that never finished pairing, so:
   **BleHidConfig::identity** gives a random static address made from the base MAC + identity
   number. "Forget Bluetooth hosts" steps it, so every host sees a new device.
   (`ESP_MAC_BT` reads zeros on the P4, because its radio is the C6; the base MAC is used.)
2. **PnP ID byte order:** `BLEHIDDevice::pnp()` packs VID/PID big-endian, the spec says little;
   the Mac showed 3A30/F082. Swapped before the call.
3. **The pairing result was invisible:** the library reports it through
   `onAuthenticationComplete(desc, status)` (two args); its own `log_e` with the SMP reason is
   compiled out. Overridden, with the status decoded (0x400+reason = our SM, 0x500+ = host's).
4. **Secure Connections fails our own DHKey check (0x40B)** with NimBLE over esp-hosted on the P4.
   **LE Legacy Just Works** (`setAuthenticationMode(true, false, false)`) works:
   `encrypted (bonded 1, key size 16)` on the Mac, 2026-11-01 ~14:33.
- macOS settings: the old addresses stay listed as extra "Tab5 Multitool" / "Bluetooth Device"
  entries for a while. Idea: put the last 4 hex digits of the address in the BLE name.
- **Verified by Donnie:** with USB unplugged, typing over Bluetooth works.
- Not yet checked: pointer over BLE, reconnect after a restart, Caps LED over BLE.

### Round 5: Mouse mode crashed over Bluetooth
- Every touch in Mouse mode panicked. The coredump (flash 0xFF0000, `esp_coredump info_corefile`)
  showed `assert getService() != nullptr` in `BLECharacteristic::notify` from `t5m_ble`.
- Cause, in the core's BLE library under NimBLE: `BLEService::addCharacteristic` won't keep a second
  characteristic with the same UUID (it revives the first). HID over GATT has one 0x2A4D per
  report, so only the keyboard input report existed. Mouse, consumer and the LED output report were
  orphans. Typing worked because it is the first report.
- Fix: `BleHid` no longer uses `BLEHIDDevice`. It registers its own GATT table with NimBLE
  (`ble_gatts_count_cfg` / `ble_gatts_add_svcs`) between `createServer()` (which resets the table)
  and the first advertising (which starts the server): DIS (manufacturer, PnP little-endian), HID
  (info, report map, control point, protocol mode, three input reports and the LED output with 0x2908
  references, reports need encryption), Battery. Notifies via `ble_gatts_notify_custom`.
- Reading a coredump: `j` on the console, then
  `esptool read-flash 0xff0000 0x10000 core.bin`, then
  `python -m esp_coredump --chip esp32p4 info_corefile --core core.bin --core-format raw firmware.elf`
  (with tool-riscv32-esp-elf-gdb on PATH).

- First try after the fix: keyboard worked over BLE but the mouse didn't. The Mac had the old
  GATT table cached from the bond (keyboard report only). Also `status 13` (BLE_HS_ETIMEOUT) on the
  reconnect's encryption. **Forget + re-pair on the Mac fixed it: keyboard and mouse both work over
  Bluetooth only** (Donnie, 2026-11-01). Reconnect-after-restart also seen working (connected at boot).
- Lesson: any change to the GATT table needs hosts to forget + re-pair (or a new identity).

### Waiting on Donnie
0. Macropad: open http://tab5-multitool.local/ (or the URL in Setup), edit, save; tap the buttons.
1. Power-cycle the Tab5 (double-press power, then press). `tools/flash.py app --wait --full` was left
   running and flashes the app as soon as the JTAG port shows up.
2. Unlock the Mac and allow "Tab5 Multitool" when macOS asks.
3. Type on the clip-on keyboard / on-screen keys into a text field; try the trackpad.
4. Look at the keyboard LEDs: is 6 % dim enough?
5. Pair Bluetooth (setting: Connection → Bluetooth) from the Mac or a phone.
