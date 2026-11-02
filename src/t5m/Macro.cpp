#include "Macro.h"

#include <ArduinoJson.h>

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace t5m {
namespace {

struct Name {
  const char *name;
  uint8_t usage;
};
const Name kKeyNames[] = {
    {"enter", key::Enter},       {"return", key::Enter},      {"esc", key::Escape},
    {"escape", key::Escape},     {"tab", key::Tab},           {"space", key::Space},
    {"backspace", key::Backspace}, {"delete", key::Delete},   {"del", key::Delete},
    {"insert", key::Insert},     {"ins", key::Insert},        {"home", key::Home},
    {"end", key::End},           {"pageup", key::PageUp},     {"pgup", key::PageUp},
    {"pagedown", key::PageDown}, {"pgdn", key::PageDown},     {"up", key::Up},
    {"down", key::Down},         {"left", key::Left},         {"right", key::Right},
    {"capslock", key::CapsLock}, {"printscreen", key::PrintScreen}, {"prtsc", key::PrintScreen},
    {"menu", key::Application},  {"pause", key::Pause},       {"numlock", key::NumLock},
    {"minus", key::Minus},       {"equal", key::Equal},       {"plus", key::Equal},
    {"kpenter", key::KpEnter},   {"kpplus", key::KpPlus},     {"kpminus", key::KpMinus},
    {"kpdot", key::KpDot},
};
const Name kModNames[] = {
    {"ctrl", mod::LCtrl},  {"control", mod::LCtrl}, {"shift", mod::LShift}, {"alt", mod::LAlt},
    {"opt", mod::LAlt},    {"option", mod::LAlt},   {"cmd", mod::LGui},     {"command", mod::LGui},
    {"gui", mod::LGui},    {"win", mod::LGui},      {"meta", mod::LGui},    {"super", mod::LGui},
    {"rctrl", mod::RCtrl}, {"rshift", mod::RShift}, {"ralt", mod::RAlt},    {"rcmd", mod::RGui},
};
struct MediaName {
  const char *name;
  uint16_t usage;
};
const MediaName kMediaNames[] = {
    {"volup", consumer::VolumeUp},     {"voldown", consumer::VolumeDown}, {"mute", consumer::Mute},
    {"play", consumer::PlayPause},     {"pause", consumer::PlayPause},    {"playpause", consumer::PlayPause},
    {"next", consumer::Next},          {"prev", consumer::Prev},          {"previous", consumer::Prev},
    {"stop", consumer::Stop},          {"brightup", consumer::BrightnessUp},
    {"brightdown", consumer::BrightnessDown},
};

std::string lower(const char *s, size_t n) {
  std::string out(s, n);
  for (char &c : out) c = (char)std::tolower((unsigned char)c);
  return out;
}

bool keyUsage(const std::string &name, uint8_t *usage) {
  if (name.size() == 1) {
    bool shift;
    // A single character: letters, digits and the unshifted punctuation keys.
    if (asciiToUsage(name[0], usage, &shift) && !shift) return true;
    return false;
  }
  if ((name[0] == 'f') && name.size() <= 3 && std::isdigit((unsigned char)name[1])) {
    const int n = std::atoi(name.c_str() + 1);
    if (n >= 1 && n <= 24) { *usage = key::f(n); return true; }
  }
  if (name.size() == 3 && name[0] == 'k' && name[1] == 'p' && std::isdigit((unsigned char)name[2])) {
    const int d = name[2] - '0';
    *usage = d == 0 ? key::Kp1 + 9 : key::Kp1 + d - 1;
    return true;
  }
  for (const auto &k : kKeyNames) if (name == k.name) { *usage = k.usage; return true; }
  return false;
}

uint32_t parseColor(const char *s, uint32_t fallback) {
  if (!s) return fallback;
  if (*s == '#') s++;
  if (std::strlen(s) != 6) return fallback;
  char *end = nullptr;
  const unsigned long v = std::strtoul(s, &end, 16);
  return end && *end == 0 ? (uint32_t)v : fallback;
}

}  // namespace

bool parseCombo(const char *s, uint8_t *mods, uint8_t keys[6], std::string *error) {
  *mods = 0;
  std::memset(keys, 0, 6);
  int nkeys = 0;
  const char *p = s;
  if (!*p) { *error = "empty key combo"; return false; }
  while (*p) {
    // A '+' as the key itself: "ctrl++" or a lone "+".
    const char *end = std::strchr(p + 1, '+');
    const size_t n = end ? (size_t)(end - p) : std::strlen(p);
    std::string tok = lower(p, n);
    bool found = false;
    for (const auto &m : kModNames) if (tok == m.name) { *mods |= m.usage; found = true; }
    if (!found) {
      uint8_t u;
      if (tok == "+") { u = key::Equal; *mods |= mod::LShift; }
      else if (!keyUsage(tok, &u)) { *error = "unknown key \"" + tok + "\" in \"" + s + "\""; return false; }
      if (nkeys == 6) { *error = std::string("more than six keys in \"") + s + "\""; return false; }
      keys[nkeys++] = u;
    }
    p += n;
    if (*p == '+') p++;
  }
  return true;
}

bool parseStep(const char *s, MacroStep *out, std::string *error) {
  *out = MacroStep();
  auto prefixed = [&](const char *pre, const char **rest) {
    const size_t n = std::strlen(pre);
    if (std::strncmp(s, pre, n) != 0) return false;
    *rest = s + n;
    return true;
  };
  const char *rest;
  if (prefixed("text:", &rest)) {
    out->kind = MacroStep::Text;
    out->text = rest;
    return true;
  }
  if (prefixed("delay:", &rest)) {
    out->kind = MacroStep::Delay;
    out->ms = (uint32_t)std::strtoul(rest, nullptr, 10);
    if (out->ms > 60000) { *error = "delay longer than 60 s"; return false; }
    return true;
  }
  if (prefixed("media:", &rest)) {
    out->kind = MacroStep::Media;
    const std::string name = lower(rest, std::strlen(rest));
    for (const auto &m : kMediaNames) if (name == m.name) { out->media = m.usage; return true; }
    *error = "unknown media key \"" + name + "\"";
    return false;
  }
  if (std::strncmp(s, "click", 5) == 0 && (s[5] == 0 || s[5] == ':')) {
    out->kind = MacroStep::Click;
    const std::string which = s[5] ? lower(s + 6, std::strlen(s + 6)) : "left";
    out->buttons = which == "right" ? button::Right : which == "middle" ? button::Middle : button::Left;
    return true;
  }
  out->kind = MacroStep::Keys;
  return parseCombo(s, &out->mods, out->keys, error);
}

bool MacroLayout::parse(const char *json, std::string *error) {
  JsonDocument doc;
  const DeserializationError e = deserializeJson(doc, json);
  if (e) { *error = std::string("JSON: ") + e.c_str(); return false; }
  JsonArrayConst pagesIn = doc["pages"].as<JsonArrayConst>();
  if (pagesIn.isNull() || pagesIn.size() == 0) { *error = "no \"pages\" list"; return false; }

  std::vector<MacroPage> pagesOut;
  int pi = 0;
  for (JsonObjectConst pj : pagesIn) {
    pi++;
    MacroPage page;
    page.name = pj["name"] | ("Page " + std::to_string(pi)).c_str();
    page.cols = pj["cols"] | 4;
    page.rows = pj["rows"] | 3;
    if (page.cols < 1 || page.cols > 8 || page.rows < 1 || page.rows > 6) {
      *error = "page " + std::to_string(pi) + ": cols must be 1-8 and rows 1-6";
      return false;
    }
    int bi = 0;
    for (JsonObjectConst bj : pj["buttons"].as<JsonArrayConst>()) {
      bi++;
      if ((int)page.buttons.size() >= page.cols * page.rows) {
        *error = "page \"" + page.name + "\": more buttons than " + std::to_string(page.cols) + "x" +
                 std::to_string(page.rows) + " slots";
        return false;
      }
      MacroButton b;
      b.label = bj["label"] | "";
      b.color = parseColor(bj["color"] | (const char *)nullptr, b.color);
      auto addStep = [&](JsonVariantConst v) {
        MacroStep st;
        std::string why;
        bool ok = false;
        if (v.is<const char *>()) {
          ok = parseStep(v.as<const char *>(), &st, &why);
        } else if (v.is<JsonObjectConst>()) {
          JsonObjectConst o = v.as<JsonObjectConst>();
          if (o["keys"].is<const char *>()) ok = parseStep(o["keys"].as<const char *>(), &st, &why);
          else if (o["text"].is<const char *>()) { st.kind = MacroStep::Text; st.text = o["text"].as<const char *>(); ok = true; }
          else if (o["media"].is<const char *>()) ok = parseStep(("media:" + std::string(o["media"].as<const char *>())).c_str(), &st, &why);
          else if (o["delay"].is<int>()) ok = parseStep(("delay:" + std::to_string(o["delay"].as<int>())).c_str(), &st, &why);
          else if (o["click"].is<const char *>()) ok = parseStep(("click:" + std::string(o["click"].as<const char *>())).c_str(), &st, &why);
          else why = "a step object needs keys, text, media, delay or click";
        } else {
          why = "a step must be a string or an object";
        }
        if (!ok) {
          *error = "page \"" + page.name + "\", button " + std::to_string(bi) +
                   (b.label.empty() ? "" : " (\"" + b.label + "\")") + ": " + why;
          return false;
        }
        b.steps.push_back(st);
        return true;
      };
      JsonVariantConst d = bj["do"];
      if (d.is<JsonArrayConst>()) {
        for (JsonVariantConst v : d.as<JsonArrayConst>()) if (!addStep(v)) return false;
      } else if (!d.isNull()) {
        if (!addStep(d)) return false;
      }
      page.buttons.push_back(std::move(b));
    }
    pagesOut.push_back(std::move(page));
  }
  pages = std::move(pagesOut);
  return true;
}

const char *MacroLayout::example() {
  return R"json({
  "pages": [
    { "name": "Shortcuts", "cols": 4, "rows": 3, "buttons": [
      { "label": "Undo", "color": "#5A4FCF", "do": "cmd+z" },
      { "label": "Redo", "color": "#5A4FCF", "do": "cmd+shift+z" },
      { "label": "Cut", "color": "#2F7BD8", "do": "cmd+x" },
      { "label": "Copy", "color": "#2F7BD8", "do": "cmd+c" },
      { "label": "Paste", "color": "#2F7BD8", "do": "cmd+v" },
      { "label": "Select\nall", "color": "#2F7BD8", "do": "cmd+a" },
      { "label": "Find", "color": "#1F9E89", "do": "cmd+f" },
      { "label": "Save", "color": "#1F9E89", "do": "cmd+s" },
      { "label": "New tab", "color": "#C7792E", "do": "cmd+t" },
      { "label": "Close tab", "color": "#C7792E", "do": "cmd+w" },
      { "label": "Reopen\ntab", "color": "#C7792E", "do": "cmd+shift+t" },
      { "label": "Spotlight", "color": "#8A3F5E", "do": "cmd+space" }
    ] },
    { "name": "Media", "cols": 3, "rows": 2, "buttons": [
      { "label": "Prev", "color": "#1F9E89", "do": "media:prev" },
      { "label": "Play /\nPause", "color": "#1F9E89", "do": "media:play" },
      { "label": "Next", "color": "#1F9E89", "do": "media:next" },
      { "label": "Vol -", "color": "#2F7BD8", "do": "media:voldown" },
      { "label": "Mute", "color": "#8A3F5E", "do": "media:mute" },
      { "label": "Vol +", "color": "#2F7BD8", "do": "media:volup" }
    ] },
    { "name": "Apps", "cols": 4, "rows": 2, "buttons": [
      { "label": "Terminal", "color": "#3A434D", "do": ["cmd+space", "delay:350", "text:Terminal\n"] },
      { "label": "Safari", "color": "#3A434D", "do": ["cmd+space", "delay:350", "text:Safari\n"] },
      { "label": "Mail", "color": "#3A434D", "do": ["cmd+space", "delay:350", "text:Mail\n"] },
      { "label": "Notes", "color": "#3A434D", "do": ["cmd+space", "delay:350", "text:Notes\n"] },
      { "label": "Screen-\nshot", "color": "#5A4FCF", "do": "cmd+shift+4" },
      { "label": "Lock", "color": "#B03A3A", "do": "ctrl+cmd+q" },
      { "label": "" },
      { "label": "Mission\nControl", "color": "#5A4FCF", "do": "ctrl+up" }
    ] }
  ]
})json";
}

void MacroRunner::run(const MacroButton &b) {
  if (queue_.size() > 200) return;  // someone is hammering a long macro
  queue_.insert(queue_.end(), b.steps.begin(), b.steps.end());
}

void MacroRunner::tick(uint32_t now) {
  while (!queue_.empty()) {
    if (waitUntil_ && (int32_t)(now - waitUntil_) < 0) return;
    waitUntil_ = 0;
    const MacroStep st = queue_.front();
    queue_.erase(queue_.begin());
    switch (st.kind) {
      case MacroStep::Keys:
        sink_.setModifiers(st.mods);
        for (uint8_t k : st.keys) if (k) sink_.press(k);
        for (uint8_t k : st.keys) if (k) sink_.release(k);
        sink_.setModifiers(0);
        break;
      case MacroStep::Text: sink_.typeText(st.text.c_str()); break;
      case MacroStep::Media: sink_.consumerTap(st.media); break;
      case MacroStep::Click: sink_.click(st.buttons); break;
      case MacroStep::Delay:
        waitUntil_ = now + st.ms;
        if (!waitUntil_) waitUntil_ = 1;
        return;
    }
  }
}

}  // namespace t5m
