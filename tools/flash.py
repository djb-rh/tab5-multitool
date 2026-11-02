#!/usr/bin/env python3
"""Flash the Tab5 Multitool, whatever the Tab5's USB-C is doing right now.

While the Multitool runs USB HID, USB-C is a keyboard + CDC console, not
USB-Serial-JTAG, so esptool can't reach it. This builds the env, sends 'j'
to the console if it is there (restart into USB-Serial-JTAG for one boot),
waits for the JTAG port, and writes the app with esptool straight away -
fast enough to catch the dev build's 3 s window after a power cycle.

    tools/flash.py [env] [--wait] [--full]

--wait  keep waiting for the JTAG port (then power-cycle the Tab5)
--full  write bootloader + partition table too (first flash on a new Tab5)
"""
import os
import subprocess
import sys
import time

HOME = os.path.expanduser("~")
PIO = f"{HOME}/.platformio/penv/bin/pio"
ESPTOOL = f"{HOME}/.platformio/penv/bin/esptool"
HERE = os.path.dirname(os.path.abspath(__file__))
FIRMWARE = os.path.normpath(os.path.join(HERE, "..", "firmware"))
VID = 0x303A
PID_JTAG = 0x1001
PID_MULTITOOL = 0x82F0


def find(pid):
    """The port with this USB ID; T5M_PORT picks one when several match.

    Other Espressif boards show up as 303A:1001 too, so with more than one
    this refuses to guess."""
    from serial.tools import list_ports
    want = os.environ.get("T5M_PORT")
    found = [p.device for p in list_ports.comports() if p.vid == VID and p.pid == pid]
    if want:
        return want if want in found else None
    if len(found) > 1:
        sys.exit("several Espressif devices are connected (%s): set T5M_PORT to the Tab5's" % ", ".join(found))
    return found[0] if found else None


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    env = args[0] if args else "app"
    wait, full = "--wait" in sys.argv, "--full" in sys.argv
    if subprocess.call([PIO, "run", "-e", env], cwd=FIRMWARE, stdout=subprocess.DEVNULL) != 0:
        sys.exit("build failed")
    build = os.path.join(FIRMWARE, ".pio", "build", env)

    jtag = find(PID_JTAG)
    if not jtag:
        cdc = find(PID_MULTITOOL)
        if cdc:
            import serial
            print(f"multitool console on {cdc}: asking for USB-Serial-JTAG")
            with serial.Serial(cdc, 115200, timeout=1) as s:
                s.write(b"j")
                time.sleep(0.3)
    if not jtag:
        print("waiting for the Tab5's USB-Serial-JTAG port" + (" (power-cycle it now)" if wait else ""))
    deadline = time.time() + (10**9 if wait else 20)
    while not jtag and time.time() < deadline:
        time.sleep(0.05)
        jtag = find(PID_JTAG)
    if not jtag:
        sys.exit("no USB-Serial-JTAG port (power-cycle the Tab5, or hold the power button for download mode)")
    print(f"USB-Serial-JTAG on {jtag}: writing {env}")
    parts = ["0x10000", os.path.join(build, "firmware.bin")]
    if full:
        parts = ["0x2000", os.path.join(build, "bootloader.bin"), "0x8000", os.path.join(build, "partitions.bin"),
                 "0xe000", os.path.join(HOME, ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin")] + parts
    # --chip: never write this to some other Espressif board.
    rc = subprocess.call([ESPTOOL, "--chip", "esp32p4", "--port", jtag, "-b", "921600", "write-flash"] + parts)
    # esptool's hard reset can leave the P4 held in reset until the port is
    # opened again with RTS/DTR released.
    # The port re-enumerates after the reset, so keep trying for a while.
    import serial
    # esptool's reset leaves it waiting; opening the port (once it has
    # re-enumerated) reboots it into the app.
    time.sleep(3.0)
    for _ in range(30):
        try:
            s = serial.Serial()
            s.port, s.rts, s.dtr, s.timeout = jtag, False, False, 0.2
            s.open()
            end = time.time() + 5
            while time.time() < end:
                s.read(4096)
            s.close()
            break
        except (serial.SerialException, OSError):
            time.sleep(0.2)
    sys.exit(rc)


if __name__ == "__main__":
    main()
