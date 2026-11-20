/* curses.h fallback for the Emscripten build. This directory is on the
 * include path only for that build (see utw.cmake).
 *
 * It exists so that unchanged #include <curses.h> lines resolve when no
 * curses backend is selected. A backend puts its real header in front of
 * this directory: wasm/curses/curses.h for UTW_CURSES=ON, or PDCursesMod's
 * curses.h for UTW_CURSES=pdcurses. Keep this stub declaration-free so the
 * declarations stay in those headers (and termcap's in tui.h).
 */
#ifndef UTW_STUB_CURSES_H
#define UTW_STUB_CURSES_H
#endif
