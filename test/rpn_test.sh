#!/bin/sh
# Host test for the calculator engine (firmware/src/app/rpn.cpp), built the
# way the firmware is: C++17, no exceptions, no RTTI.
set -e
cd "$(dirname "$0")"
mkdir -p build
clang++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -O1 -o build/rpn_test rpn_test.cpp ../firmware/src/app/rpn.cpp
./build/rpn_test
