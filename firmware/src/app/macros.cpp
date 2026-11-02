#include "macros.h"

#include <LittleFS.h>

namespace mt {
namespace {

constexpr const char *kPath = "/macros.json";
t5m::MacroLayout g_layout;
std::string g_json;
uint32_t g_version = 1;
bool g_fs = false;

void useExample() {
  std::string err;
  g_json = t5m::MacroLayout::example();
  g_layout.parse(g_json.c_str(), &err);
}

}  // namespace

void macrosBegin() {
  // Partition "littlefs" (16 MB table); formatted the first time.
  g_fs = LittleFS.begin(true, "/littlefs", 10, "littlefs");
  if (g_fs && LittleFS.exists(kPath)) {
    File f = LittleFS.open(kPath, "r");
    std::string json;
    json.reserve(f.size());
    while (f.available()) {
      char buf[256];
      const int n = f.readBytes(buf, sizeof buf);
      json.append(buf, n);
    }
    f.close();
    std::string err;
    if (g_layout.parse(json.c_str(), &err)) {
      g_json = json;
      return;
    }
    Serial.printf("macros: saved layout is bad (%s), using the example\n", err.c_str());
  }
  useExample();
}

const t5m::MacroLayout &macros() { return g_layout; }
const std::string &macrosJson() { return g_json; }
uint32_t macrosVersion() { return g_version; }

bool macrosSave(const std::string &json, std::string *error) {
  t5m::MacroLayout next;
  if (!next.parse(json.c_str(), error)) return false;
  if (!g_fs) { *error = "no filesystem"; return false; }
  File f = LittleFS.open(kPath, "w");
  if (!f || f.write((const uint8_t *)json.data(), json.size()) != json.size()) {
    *error = "could not write the file";
    return false;
  }
  f.close();
  g_layout = std::move(next);
  g_json = json;
  g_version++;
  return true;
}

void macrosReset() {
  if (g_fs) LittleFS.remove(kPath);
  useExample();
  g_version++;
}

}  // namespace mt
