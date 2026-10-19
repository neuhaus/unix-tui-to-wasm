/* demo.c - minimal TUI using the termcap shims, for smoke-testing the
 * template. Builds natively (prints ANSI escapes to the current terminal)
 * and as the Emscripten page.
 */
#include <stdio.h>

#include "tui.h"

static int putch(int c) { return putchar(c); }

int main(void) {
  char *cm, *cl, *so, *se;

  tgetent(NULL, "xterm");
  cl = tgetstr("cl", NULL);
  cm = tgetstr("cm", NULL);
  so = tgetstr("so", NULL);
  se = tgetstr("se", NULL);

  if (cl) tputs(cl, 1, putch);

  tputs(tgoto(cm, 2, 1), 1, putch);
  if (so) tputs(so, 1, putch);
  fputs("unix-tui-to-wasm", stdout);
  if (se) tputs(se, 1, putch);

  tputs(tgoto(cm, 2, 3), 1, putch);
  printf("terminal: %dx%d", tgetnum("co"), tgetnum("li"));

  tputs(tgoto(cm, 2, 5), 1, putch);
  fputs("press Enter to quit", stdout);
  fflush(stdout);

  while (getchar() != '\n')
    ;

  if (cl) tputs(cl, 1, putch);
  return 0;
}
