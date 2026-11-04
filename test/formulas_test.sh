#!/bin/sh
# Host test for the Formulas engine and library (firmware/src/app/formula*.cpp). Built
# the way the firmware builds it: C++17, no exceptions, no RTTI.
set -e
cd "$(dirname "$0")"
mkdir -p build
clang++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -O1 -o build/formulas_test formulas_test.cpp \
  ../firmware/src/app/formulas.cpp ../firmware/src/app/formula_library.cpp \
  ../firmware/src/app/formula_lib_*.cpp ../firmware/src/app/units.cpp
./build/formulas_test "$@"
