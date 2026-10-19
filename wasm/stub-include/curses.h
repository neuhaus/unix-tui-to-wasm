/* curses.h stub for the Emscripten build. This directory is on the include
 * path only for that build (see utw.cmake).
 *
 * Emscripten ships no curses and the terminal is xterm.js, so a ported
 * program makes no curses calls. The one symbol these sources expect from a
 * curses header - delay_output() - is declared in tui.h and implemented in
 * tui.c. Keep this stub declaration-free so the declarations stay in one
 * place.
 */
#ifndef UTW_STUB_CURSES_H
#define UTW_STUB_CURSES_H
#endif
