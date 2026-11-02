#!/bin/sh
# Builds the Swift side for this Mac and runs the terminal interface tests.
set -e
cd "$(dirname "$0")/.."
swift/build.sh host >/dev/null
TOOLCHAIN=${TOOLCHAIN:-$HOME/Library/Developer/Toolchains/swift-latest.xctoolchain}
mkdir -p test/build
clang -O1 -Iswift/include test/term_test.c swift/.build/host/libt5mswift.a \
  "$TOOLCHAIN/usr/lib/swift/embedded/arm64-apple-macos/libswiftUnicodeDataTables.a" \
  -o test/build/term_test
test/build/term_test
