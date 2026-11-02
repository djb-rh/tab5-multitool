#!/bin/sh
# Host tests (no device). ArduinoJson comes from the firmware's PlatformIO deps.
set -e
cd "$(dirname "$0")"
AJ=$(ls -d ../firmware/.pio/libdeps/app/ArduinoJson/src | head -1)
mkdir -p build
clang++ -std=c++17 -Wall -I"$AJ" -o build/macro_test macro_test.cpp ../src/t5m/Macro.cpp ../src/t5m/HidSink.cpp
./build/macro_test
