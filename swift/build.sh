#!/bin/sh
# Builds the Swift side of the firmware (Embedded Swift): SwiftTerm's Embedded
# core plus our C interface to it (include/t5m_term.h).
#
#   ./build.sh          for the Tab5 -> firmware/lib/t5mswift/libt5mswift.a
#   ./build.sh host     for this Mac  -> .build/host/libt5mswift.a (test/term_test.sh)
#
# Needs a Swift.org toolchain (6.3+; Xcode's has no RISC-V Embedded stdlib):
#   swiftly install latest      (or set TOOLCHAIN)
set -e
cd "$(dirname "$0")"
TARGET=${1:-p4}
TOOLCHAIN=${TOOLCHAIN:-$HOME/Library/Developer/Toolchains/swift-latest.xctoolchain}
SWIFTC=$TOOLCHAIN/usr/bin/swiftc
ST=../third_party/SwiftTerm/Sources/SwiftTerm
XT=$HOME/.platformio/packages/toolchain-riscv32-esp/bin/riscv32-esp-elf
BUILD=.build/$TARGET
mkdir -p "$BUILD"

if [ "$TARGET" = p4 ]; then
  # The P4 is RV32IMAFC with the single-float ABI (ilp32f), like the Arduino libs.
  TRIPLE=riscv32-none-none-eabi
  FLAGS="-Xcc -march=rv32imafc_zicsr_zifencei -Xcc -mabi=ilp32f -Xcc -fno-pic -Xcc -fno-pie
         -Xfrontend -function-sections -Xfrontend -disable-stack-protector"
  AR=$XT-ar
  OUT=../firmware/lib/t5mswift
else
  TRIPLE=arm64-apple-macosx14.0
  FLAGS=""
  AR=ar
  OUT=$BUILD
fi
EMB=$TOOLCHAIN/usr/lib/swift/embedded/$TRIPLE
[ -d "$EMB" ] || EMB=$TOOLCHAIN/usr/lib/swift/embedded/${TRIPLE%%14.0}
COMMON="-target $TRIPLE -enable-experimental-feature Embedded -wmo -parse-as-library -Osize $FLAGS"

# SwiftTerm (slow, ~30 s): only when its sources change. Apple/Mac/iOS are UI.
ST_SRCS=$(find "$ST" -name '*.swift' -not -path '*/Apple/*' -not -path '*/Mac/*' -not -path '*/iOS/*')
if [ ! -f "$BUILD/SwiftTerm.o" ] || [ -n "$(find "$ST" Generated -name '*.swift' -newer "$BUILD/SwiftTerm.o" | head -1)" ]; then
  echo "compiling SwiftTerm for $TARGET"
  # shellcheck disable=SC2086
  "$SWIFTC" $COMMON -module-name SwiftTerm -D SWIFTTERM_EMBEDDED \
    -emit-module -emit-module-path "$BUILD/SwiftTerm.swiftmodule" \
    -c $ST_SRCS Generated/*.swift -o "$BUILD/SwiftTerm.o"
fi

# shellcheck disable=SC2086
"$SWIFTC" $COMMON -module-name T5MSwift -I "$BUILD" -import-objc-header include/t5m_term.h \
  -c Sources/T5MSwift/*.swift -o "$BUILD/t5mswift.o"

OBJS="$BUILD/SwiftTerm.o $BUILD/t5mswift.o"
if [ "$TARGET" = p4 ]; then
  # The IDF linker script discards .got and doesn't place .swift_modhash.
  for o in $OBJS; do
    cp "$o" "$o.fw"
    python3 fix_got.py "$o.fw"
    "$XT-objcopy" --remove-section=.swift_modhash "$o.fw"
  done
  OBJS="$BUILD/SwiftTerm.o.fw $BUILD/t5mswift.o.fw"
  # The stdlib's Unicode tables ship built for the soft-float ABI. They are
  # only data, so mark them single-float (ELF e_flags 0x1 -> 0x3) or the link
  # refuses to mix them with everything else.
  rm -rf "$BUILD/unicode" && mkdir -p "$BUILD/unicode"
  (cd "$BUILD/unicode" && "$AR" x "$EMB/libswiftUnicodeDataTables.a")
  python3 - "$BUILD"/unicode/*.o <<'PY'
import struct, sys
for path in sys.argv[1:]:
    b = bytearray(open(path, "rb").read())
    assert b[:4] == b"\x7fELF" and b[4] == 1, path
    flags = struct.unpack_from("<I", b, 0x24)[0]
    struct.pack_into("<I", b, 0x24, (flags & ~0x6) | 0x2)  # float ABI = single
    open(path, "wb").write(b)
PY
  OBJS="$OBJS $BUILD/unicode/*.o"
fi

mkdir -p "$OUT"
rm -f "$OUT/libt5mswift.a"
# shellcheck disable=SC2086
"$AR" rcs "$OUT/libt5mswift.a" $OBJS
echo "built $OUT/libt5mswift.a"
