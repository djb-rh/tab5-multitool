#!/usr/bin/env python3
"""Screenshot of the Tab5 Multitool's screen, as a PNG.

    tools/shot.py out.png [commands]

Opens the console (the CDC port while USB HID runs, else USB-Serial-JTAG -
which restarts the Tab5, so this waits for it to boot), sends any commands
(e.g. "5" for Macros mode), then "P", and writes what comes back.
"""
import os
import struct
import sys
import time
import zlib

import serial
from serial.tools import list_ports


def port():
    # Multitool CDC console, then USB-Serial-JTAG. Other Espressif boards are
    # 303A:1001 too: with several, T5M_PORT says which is the Tab5.
    pick = os.environ.get("T5M_PORT")
    for want in (0x82F0, 0x1001):
        found = [p.device for p in list_ports.comports() if p.vid == 0x303A and p.pid == want]
        if pick:
            found = [d for d in found if d == pick]
        if len(found) > 1:
            sys.exit("several Espressif devices are connected (%s): set T5M_PORT to the Tab5's" % ", ".join(found))
        if found:
            return found[0], want == 0x1001
    sys.exit("no Tab5 found")


def png(path, w, h, rgb565):
    rows = bytearray()
    for y in range(h):
        rows.append(0)
        line = rgb565[y * w * 2:(y + 1) * w * 2]
        for i in range(0, len(line), 2):
            v = (line[i] << 8) | line[i + 1]  # big-endian RGB565
            rows += bytes(((v >> 8) & 0xF8 | (v >> 13), (v >> 3) & 0xFC | ((v >> 9) & 3), (v << 3) & 0xF8 | ((v >> 2) & 7)))

    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(bytes(rows), 6)) + chunk(b"IEND", b""))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "shot.png"
    cmds = sys.argv[2] if len(sys.argv) > 2 else ""
    dev, jtag = port()
    s = serial.Serial()
    s.port, s.baudrate, s.timeout, s.rts, s.dtr = dev, 115200, 0.5, False, False
    s.open()
    if jtag:
        time.sleep(9)  # opening USB-Serial-JTAG restarts it; let it boot
    s.reset_input_buffer()
    for c in cmds:
        s.write(c.encode())
        time.sleep(0.6)
    s.reset_input_buffer()
    s.write(b"P")
    buf = b""
    deadline = time.time() + 30
    while b"SHOT " not in buf and time.time() < deadline:
        buf += s.read(4096)
    head = buf[buf.index(b"SHOT "):]
    line, rest = head.split(b"\n", 1)
    w, h = map(int, line.split()[1:3])
    need = w * h * 2
    while len(rest) < need and time.time() < deadline:
        rest += s.read(65536)
    if len(rest) < need:
        sys.exit(f"short read: {len(rest)} of {need} bytes")
    png(out, w, h, rest[:need])
    print(f"{out}: {w}x{h}")


if __name__ == "__main__":
    main()
