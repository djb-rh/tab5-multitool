// C interface to SwiftTerm (Embedded Swift, swift/Sources/T5MSwift/Term.swift).
// One thread at a time per terminal: the Embedded core has no lock.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// One screen cell. Colours are 0xRRGGBB with inverse already applied.
typedef struct {
  uint32_t ch;      // first Unicode scalar of the cell's text; 0 = empty
  uint32_t fg, bg;
  uint8_t style;    // SwiftTerm CharacterStyle bits: 1 bold, 2 underline, 4 blink,
                    // 8 inverse, 16 invisible, 32 dim, 64 italic, 128 crossed out
  uint8_t width;    // 1, 2 for a wide character, 0 for the cell after a wide one
  uint8_t pad[2];
} t5m_cell;

typedef struct {
  int32_t x, y;     // viewport cell
  uint8_t visible;
  uint8_t shape;    // 0 block, 1 underline, 2 bar
  uint8_t blink;
  uint8_t pad;
  uint32_t color;
} t5m_cursor;

typedef void (*t5m_row_fn)(void *ctx, int y, const t5m_cell *cells, int n);

void *t5m_term_new(int cols, int rows, int scrollback);
void t5m_term_free(void *term);
void t5m_term_resize(void *term, int cols, int rows);

// Output from the peer (SSH, serial) to show.
void t5m_term_feed(void *term, const uint8_t *data, int n);
// Typed text (UTF-8) and named keys (W3C KeyboardEvent key + code, e.g.
// "ArrowUp"/"ArrowUp", "c"/"KeyC"); mods: 1 shift, 2 alt, 4 ctrl, 8 super.
// Both queue bytes for the peer; returns 1 if anything was queued.
int t5m_term_text(void *term, const uint8_t *utf8, int n);
int t5m_term_key(void *term, const char *key, const char *code, uint32_t mods);
// Takes up to cap queued bytes for the peer; returns the count.
int t5m_term_output(void *term, uint8_t *buf, int cap);

// Calls fn for each row that changed (all rows when full), then marks them
// drawn. Returns the number of rows passed to fn. *bg is the default background.
int t5m_term_frame(void *term, int full, t5m_row_fn fn, void *ctx, t5m_cursor *cursor, uint32_t *bg);

// Scrollback: positive lines scroll toward newer output. Returns how many
// rows above the live screen the view now starts (0 = live).
int t5m_term_scroll(void *term, int lines);
void t5m_term_scroll_live(void *term);

// Title (OSC 0/2): copies it and returns a counter that grows on each change.
uint32_t t5m_term_title(void *term, char *buf, int cap);
// Bells so far.
uint32_t t5m_term_bells(void *term);

#ifdef __cplusplus
}
#endif
