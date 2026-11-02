// Macros: pages of buttons that each send something to the host.
//
// A layout is JSON, so it can be edited on a computer (or by the Tab5's own
// web editor) and stored on flash or SD:
//
//   { "pages": [ { "name": "Shortcuts", "cols": 4, "rows": 3, "buttons": [
//       { "label": "Copy",  "color": "#2F7BD8", "do": "cmd+c" },
//       { "label": "Sig",   "do": "text:Thanks,\nDonnie\n" },
//       { "label": "Vol +", "do": "media:volup" },
//       { "label": "Term",  "do": ["cmd+space", {"delay": 300}, "text:Terminal\n"] },
//       { "label": "" }                                   <- an empty slot
//   ] } ] }
//
// "do" is one step or a list of them. A step is a string -
//   "ctrl+alt+delete"  keys pressed together then released (a combo)
//   "text:..."         typed out (US layout)
//   "media:volup"      a media key (volup voldown mute play next prev stop
//                      brightup brightdown)
//   "delay:250"        wait, in milliseconds
//   "click", "click:right", "click:middle"
// - or an object with one of: keys, text, media, delay, click.
//
// Key names in a combo: ctrl shift alt (opt option) cmd (gui win meta super),
// a..z 0..9 f1..f24, enter (return) esc tab space backspace delete (del)
// insert home end pageup pagedown up down left right capslock printscreen
// menu, the punctuation keys themselves (- = [ ] \ ; ' ` , . /) and kp0..kp9.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "HidSink.h"

namespace t5m {

struct MacroStep {
  enum Kind : uint8_t { Keys, Text, Media, Delay, Click };
  Kind kind = Keys;
  uint8_t mods = 0;
  uint8_t keys[6] = {};  // Keys: up to six non-modifier keys
  uint16_t media = 0;
  uint32_t ms = 0;       // Delay
  uint8_t buttons = 0;   // Click
  std::string text;      // Text
};

struct MacroButton {
  std::string label;  // "\n" breaks the line
  uint32_t color = 0x2C333B;
  std::vector<MacroStep> steps;  // none = an empty slot
};

struct MacroPage {
  std::string name;
  int cols = 4, rows = 3;
  std::vector<MacroButton> buttons;
};

struct MacroLayout {
  std::vector<MacroPage> pages;

  // Replaces this layout. On error, returns false with a message naming the
  // page/button that is wrong, and leaves the layout as it was.
  bool parse(const char *json, std::string *error);
  // A starter layout (also what a Tab5 with no saved layout shows).
  static const char *example();
};

// One step from its string form; false (with a reason) if it isn't one.
bool parseStep(const char *s, MacroStep *out, std::string *error);
// "ctrl+shift+t" -> modifiers + keys.
bool parseCombo(const char *s, uint8_t *mods, uint8_t keys[6], std::string *error);

// Plays macros on a HidSink without blocking: run() queues a button's steps,
// tick() sends what is due (a delay step waits without holding up loop()).
class MacroRunner {
 public:
  explicit MacroRunner(HidSink &sink) : sink_(sink) {}
  void run(const MacroButton &b);
  void tick(uint32_t now_ms);
  bool busy() const { return !queue_.empty(); }
  void cancel() { queue_.clear(); }

 private:
  HidSink &sink_;
  std::vector<MacroStep> queue_;
  uint32_t waitUntil_ = 0;
};

}  // namespace t5m
