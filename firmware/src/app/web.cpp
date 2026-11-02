#include "web.h"

#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "macros.h"
#include "ssh.h"
#include "web_page.h"

namespace mt {
namespace {

WebServer g_server(80);
bool g_started = false;
bool g_wanted = false;

void sendJson(int code, const String &body) {
  g_server.sendHeader("Cache-Control", "no-store");
  g_server.send(code, "application/json", body);
}

String jsonString(const std::string &s) {
  String out = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (c == '\n') out += "\\n";
    else if ((uint8_t)c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); out += b; }
    else out += c;
  }
  return out + "\"";
}

void start() {
  g_server.on("/", HTTP_GET, [] {
    g_server.sendHeader("Cache-Control", "no-store");
    g_server.send_P(200, "text/html", kEditorPage);
  });
  g_server.on("/api/layout", HTTP_GET, [] {
    g_server.sendHeader("Cache-Control", "no-store");
    g_server.send(200, "application/json", macrosJson().c_str());
  });
  g_server.on("/api/layout", HTTP_POST, [] {
    if (!g_server.hasArg("plain")) { sendJson(400, "{\"error\":\"no body\"}"); return; }
    const String &body = g_server.arg("plain");
    std::string err;
    if (!macrosSave(std::string(body.c_str(), body.length()), &err)) {
      sendJson(400, "{\"error\":" + jsonString(err) + "}");
      return;
    }
    sendJson(200, "{\"ok\":true}");
  });
  g_server.on("/api/example", HTTP_GET, [] {
    g_server.send(200, "application/json", t5m::MacroLayout::example());
  });
  g_server.on("/api/reset", HTTP_POST, [] {
    macrosReset();
    sendJson(200, "{\"ok\":true}");
  });
  // The Terminal's SSH key, for a server's ~/.ssh/authorized_keys.
  g_server.on("/ssh-key.pub", HTTP_GET, [] {
    const char *k = ssh::publicKey();
    if (!k[0]) g_server.send(404, "text/plain", "open the Terminal app once to make the key\n");
    else g_server.send(200, "text/plain", String(k) + "\n");
  });
  g_server.onNotFound([] { g_server.send(404, "text/plain", "not found"); });
  g_server.begin();
  MDNS.begin("tab5-multitool");
  MDNS.addService("http", "tcp", 80);
  g_started = true;
}

}  // namespace

void webBegin() { g_wanted = true; }

void webTick() {
  if (!g_wanted) return;
  if (!g_started) {
    if (WiFi.status() == WL_CONNECTED) start();
    return;
  }
  g_server.handleClient();
}

bool webRunning() { return g_started; }

}  // namespace mt
