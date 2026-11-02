// Host test for the macro parser: builds with the Mac's clang, no device.
//   test/run.sh
#include <cassert>
#include <cstdio>
#include <string>

#include "../src/t5m/Macro.h"

using namespace t5m;

// Records what the runner sends, as text.
struct Log : HidSink {
  std::string s;
  uint8_t m = 0;
  void press(uint8_t u) override { s += "+" + std::to_string(u) + "/" + std::to_string(m) + " "; }
  void release(uint8_t u) override { s += "-" + std::to_string(u) + " "; }
  void releaseAll() override {}
  void setModifiers(uint8_t mods) override { m = mods; }
  void mouseMove(int, int, int, int) override {}
  void mouseButtons(uint8_t b) override { if (b) s += "click" + std::to_string(b) + " "; }
  void consumer(uint16_t u) override { if (u) s += "media" + std::to_string(u) + " "; }
  bool connected() const override { return true; }
  uint8_t leds() const override { return 0; }
};

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main() {
  std::string err;
  uint8_t mods, keys[6];

  CHECK(parseCombo("cmd+shift+t", &mods, keys, &err));
  CHECK(mods == (mod::LGui | mod::LShift) && keys[0] == key::A + ('t' - 'a') && keys[1] == 0);
  CHECK(parseCombo("CTRL+Alt+Delete", &mods, keys, &err) && mods == (mod::LCtrl | mod::LAlt) && keys[0] == key::Delete);
  CHECK(parseCombo("f12", &mods, keys, &err) && mods == 0 && keys[0] == key::f(12));
  CHECK(parseCombo("ctrl++", &mods, keys, &err) && keys[0] == key::Equal && (mods & mod::LShift));
  CHECK(parseCombo("cmd+/", &mods, keys, &err) && keys[0] == key::Slash);
  CHECK(parseCombo("kp0", &mods, keys, &err) && keys[0] == key::Kp1 + 9);
  CHECK(!parseCombo("ctrl+banana", &mods, keys, &err) && err.find("banana") != std::string::npos);

  MacroStep st;
  CHECK(parseStep("text:hi\n", &st, &err) && st.kind == MacroStep::Text && st.text == "hi\n");
  CHECK(parseStep("delay:250", &st, &err) && st.kind == MacroStep::Delay && st.ms == 250);
  CHECK(parseStep("media:volup", &st, &err) && st.media == consumer::VolumeUp);
  CHECK(parseStep("click:right", &st, &err) && st.buttons == button::Right);
  CHECK(!parseStep("media:louder", &st, &err));

  MacroLayout l;
  err.clear();
  CHECK(l.parse(MacroLayout::example(), &err));
  if (!err.empty()) printf("example: %s\n", err.c_str());
  CHECK(l.pages.size() == 3 && l.pages[0].buttons.size() == 12 && l.pages[2].buttons[6].steps.empty());
  CHECK(l.pages[0].buttons[5].label == "Select\nall");

  // Errors name the place and leave the layout alone.
  CHECK(!l.parse(R"({"pages":[{"name":"X","cols":2,"rows":1,"buttons":[{"do":"a"},{"do":"b"},{"do":"c"}]}]})", &err));
  CHECK(err.find("more buttons") != std::string::npos && l.pages.size() == 3);
  CHECK(!l.parse(R"({"pages":[{"name":"P","buttons":[{"label":"Bad","do":["cmd+c",{"wat":1}]}]}]})", &err));
  CHECK(err.find("\"Bad\"") != std::string::npos);
  CHECK(!l.parse("{not json", &err) && err.rfind("JSON", 0) == 0);

  // The runner: combos, text, media, and a delay that waits.
  CHECK(l.parse(R"({"pages":[{"buttons":[{"do":["cmd+c","delay:100","media:mute","text:Ab"]}]}]})", &err));
  Log log;
  MacroRunner r(log);
  r.run(l.pages[0].buttons[0]);
  r.tick(1000);
  CHECK(log.s == "+6/8 -6 ");  // c with cmd, then the delay starts
  CHECK(r.busy());
  r.tick(1050);
  CHECK(log.s == "+6/8 -6 ");  // still waiting
  r.tick(1101);
  CHECK(!r.busy());
  CHECK(log.s == "+6/8 -6 media226 +4/2 -4 +5/0 -5 ");  // mute, A (shift), b

  printf(failures ? "%d FAILED\n" : "all macro tests passed\n", failures);
  return failures ? 1 : 0;
}
