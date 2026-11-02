#!/bin/sh
# Builds the release files into dist/:
#   tab5-multitool-<ver>.bin        merged image (bootloader 0x2000, partitions
#                                   0x8000, app 0x10000), flashed at offset 0
#   tab5-multitool-sdcard-<ver>.zip the burner/ folder the T48 app reads from a
#                                   FAT32 SD card
# and the web installer (site/: index.html, manifest.json, the .bin) that the
# gh-pages branch serves. Usage: tools/release.sh v0.1.0
set -e
ver=${1:?usage: tools/release.sh <version>}
cd "$(dirname "$0")/.."
export PATH="$HOME/.cargo/bin:$PATH"
(cd firmware && ~/.platformio/penv/bin/pio run -e release)
bin=firmware/.pio/build/release/firmware.factory.bin
# The release build must not carry a development Wi-Fi network.
if [ -f firmware/include/secrets.h ]; then
  ssid=$(sed -n 's/.*WIFI_SSID *"\(.*\)".*/\1/p' firmware/include/secrets.h)
  pass=$(sed -n 's/.*WIFI_PASSWORD *"\(.*\)".*/\1/p' firmware/include/secrets.h)
  for s in "$ssid" "$pass"; do
    [ -n "$s" ] && grep -q -a -F "$s" "$bin" && { echo "secrets.h found in $bin" >&2; exit 1; }
  done
fi
rm -rf dist site && mkdir -p dist site
cp "$bin" "dist/tab5-multitool-$ver.bin"
t48=third_party/t48-for-tab5
python3 "$t48/tools/mkparts.py" "$t48/third_party/minipro" dist/db
python3 - "$ver" <<'PY'
import os, sys, zipfile
ver = sys.argv[1]
with zipfile.ZipFile(f"dist/tab5-multitool-sdcard-{ver}.zip", "w", zipfile.ZIP_DEFLATED) as z:
    for n in sorted(os.listdir("dist/db")):
        z.write(f"dist/db/{n}", f"burner/db/{n}")
    z.writestr("burner/images/README.txt",
               "ROM images go here. Reads from the T48 land here too.\n")
PY
rm -rf dist/db
cp "dist/tab5-multitool-$ver.bin" "site/tab5-multitool-$ver.bin"
sed -e "s/@VER@/$ver/g" web-installer/index.html > site/index.html
sed -e "s/@VER@/$ver/g" web-installer/manifest.json > site/manifest.json
cp docs/shots/home.png site/home.png
ls -la dist site
