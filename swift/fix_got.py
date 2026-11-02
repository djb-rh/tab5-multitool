#!/usr/bin/env python3
"""Rewrite GOT loads in an Embedded Swift object as direct PC-relative addresses.

Swift reaches a few C globals of its stdlib (the Unicode NFD tables) through the
GOT even when building non-PIC, and the ESP-IDF linker script discards .got.
Every such access is the pair

    auipc rd, %got_pcrel_hi(sym)     R_RISCV_GOT_HI20   sym
    lw    rd, %pcrel_lo(label)(rd)   R_RISCV_PCREL_LO12_I label (at the auipc)

which loads &sym from the GOT. In a static image that is the same as

    auipc rd, %pcrel_hi(sym)         R_RISCV_PCREL_HI20 sym
    addi  rd, rd, %pcrel_lo(label)

so this changes the relocation type and turns the lw into an addi.

    fix_got.py file.o
"""
import struct
import sys

R_GOT_HI20, R_PCREL_HI20, R_PCREL_LO12_I = 20, 23, 24


def main(path):
    b = bytearray(open(path, "rb").read())
    assert b[:4] == b"\x7fELF" and b[4] == 1 and b[5] == 1, "want ELF32 little-endian"
    shoff, = struct.unpack_from("<I", b, 0x20)
    shentsize, shnum, _ = struct.unpack_from("<HHH", b, 0x2E)
    secs = [struct.unpack_from("<IIIIIIIIII", b, shoff + i * shentsize) for i in range(shnum)]
    symtab = next(s for s in secs if s[1] == 2)  # SHT_SYMTAB
    def sym(i):
        name, value, size, info, other, shndx = struct.unpack_from("<IIIBBH", b, symtab[4] + i * 16)
        return value, shndx
    fixed = 0
    for s in secs:
        if s[1] != 4:  # SHT_RELA
            continue
        target = secs[s[7]]  # sh_info: the section the relocations apply to
        relas = [(s[4] + k * 12,) + struct.unpack_from("<IIi", b, s[4] + k * 12) for k in range(s[5] // 12)]
        for at, off, info, add in relas:
            if info & 0xFF != R_GOT_HI20:
                continue
            struct.pack_into("<I", b, at + 4, (info & ~0xFF) | R_PCREL_HI20)
            # Its partners: PCREL_LO12_I relocations against a label at this auipc.
            for _, off2, info2, _ in relas:
                if info2 & 0xFF != R_PCREL_LO12_I:
                    continue
                value, shndx = sym(info2 >> 8)
                if value != off or secs[shndx] is not target:
                    continue
                pos = target[4] + off2
                insn, = struct.unpack_from("<I", b, pos)
                assert insn & 0x707F == 0x2003, f"expected lw at {off2:#x}, got {insn:#010x}"
                struct.pack_into("<I", b, pos, (insn & ~0x707F) | 0x0013)  # lw -> addi
                fixed += 1
    open(path, "wb").write(b)
    print(f"fix_got: {fixed} GOT load(s) made direct in {path}")


if __name__ == "__main__":
    main(sys.argv[1])
