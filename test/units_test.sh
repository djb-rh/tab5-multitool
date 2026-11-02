#!/bin/sh
# Host test for the Units engine (firmware/src/app/units.cpp). Built the way the
# firmware builds it: C++17, no exceptions, no RTTI.
set -e
cd "$(dirname "$0")"
mkdir -p build
clang++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -O1 -o build/units_test units_test.cpp \
  ../firmware/src/app/units.cpp
./build/units_test
