// Host test for the SwiftTerm C interface (swift/include/t5m_term.h): feeds
// escape sequences and checks the rows a renderer would be given.
#include <stdio.h>
#include <string.h>

#include "t5m_term.h"

static char screen[4][41];
static t5m_cell row1[40];
static int rowsSeen;

static void onRow(void *ctx, int y, const t5m_cell *cells, int n) {
  (void)ctx;
  rowsSeen++;
  for (int x = 0; x < n && x < 40; x++) {
    uint32_t c = cells[x].ch;
    screen[y][x] = c == 0 ? ' ' : c < 128 ? (char)c : '#';
    if (y == 1) row1[x] = cells[x];
  }
  screen[y][n < 40 ? n : 40] = 0;
}

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void feed(void *t, const char *s) { t5m_term_feed(t, (const uint8_t *)s, (int)strlen(s)); }

int main(void) {
  void *t = t5m_term_new(40, 4, 100);
  t5m_cursor cur;
  uint32_t bg;

  // Plain text, a colour, a wide character.
  feed(t, "hello\r\n\x1b[31mred\x1b[0m \xe4\xb8\x96x");
  int n = t5m_term_frame(t, 1, onRow, NULL, &cur, &bg);
  CHECK(n == 4, "full frame gave %d rows", n);
  CHECK(strncmp(screen[0], "hello ", 6) == 0, "row 0 = '%s'", screen[0]);
  CHECK(strncmp(screen[1], "red #", 5) == 0, "row 1 = '%s'", screen[1]);
  CHECK(row1[0].fg != row1[4].fg, "red text should differ from default (%06x vs %06x)", row1[0].fg, row1[4].fg);
  CHECK(row1[4].width == 2 && row1[5].width == 0 && row1[5 + 1].ch == 'x', "wide cell widths %d %d", row1[4].width, row1[5].width);
  CHECK(cur.x == 7 && cur.y == 1, "cursor at %d,%d", cur.x, cur.y);
  printf("default bg %06x, red %06x, plain %06x\n", bg, row1[0].fg, row1[4].fg);

  // Nothing changed: a dirty frame draws nothing.
  rowsSeen = 0;
  n = t5m_term_frame(t, 0, onRow, NULL, &cur, &bg);
  CHECK(n == 0, "clean frame gave %d rows", n);

  // Only the changed row is redrawn.
  feed(t, "\x1b[4;1Hlast");
  n = t5m_term_frame(t, 0, onRow, NULL, &cur, &bg);
  CHECK(n >= 1 && n <= 3, "one-row change gave %d rows", n);  // rows 1-3: old and new cursor rows
  CHECK(strncmp(screen[3], "last", 4) == 0, "row 3 = '%s'", screen[3]);

  // Keys: arrows, Ctrl-C, typed text; a terminal reply (DA).
  uint8_t out[64];
  t5m_term_key(t, "ArrowUp", "ArrowUp", 0);
  t5m_term_key(t, "c", "KeyC", 4);
  t5m_term_text(t, (const uint8_t *)"ls\r", 3);
  n = t5m_term_output(t, out, sizeof out);
  CHECK(n == 7 && memcmp(out, "\x1b[A\x03ls\r", 7) == 0, "keys gave %d bytes", n);
  feed(t, "\x1b[?1h");  // application cursor keys
  t5m_term_key(t, "ArrowUp", "ArrowUp", 0);
  n = t5m_term_output(t, out, sizeof out);
  CHECK(n == 3 && memcmp(out, "\x1bOA", 3) == 0, "app cursor up gave %d bytes", n);
  feed(t, "\x1b[c");
  n = t5m_term_output(t, out, sizeof out);
  CHECK(n > 3 && out[0] == 0x1b, "DA reply %d bytes", n);

  // Title and bell.
  char title[32];
  feed(t, "\x1b]0;my title\x07\x07");
  uint32_t gen = t5m_term_title(t, title, sizeof title);
  CHECK(gen == 1 && strcmp(title, "my title") == 0, "title '%s' gen %u", title, gen);
  CHECK(t5m_term_bells(t) == 1, "bells %u", t5m_term_bells(t));

  // Scrollback.
  for (int i = 0; i < 20; i++) feed(t, "\r\nline");
  int back = t5m_term_scroll(t, -5);
  CHECK(back == 5, "scrolled back %d", back);
  t5m_term_scroll_live(t);
  CHECK(t5m_term_scroll(t, 0) == 0, "not live after scroll_live");

  t5m_term_free(t);
  if (failures) { printf("%d failure(s)\n", failures); return 1; }
  printf("all terminal tests passed\n");
  return 0;
}
