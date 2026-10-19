/* tui.h - termcap entry points for the browser terminal.
 *
 * Include this where a ported program calls termcap (on the Emscripten
 * build; native builds that link a real termcap should not include it).
 * The shims are implemented in tui.c as ANSI escapes that xterm.js
 * understands; xterm-pty handles the actual I/O, termios, window size and
 * Ctrl+C. See README.md for the porting recipe.
 */

#ifndef UTW_TUI_H
#define UTW_TUI_H

int wasm_tgetent(char *bp, const char *name);
char *wasm_tgetstr(const char *id, char **area);
int wasm_tgetnum(const char *id);
int wasm_tgetflag(const char *id);
char *wasm_tgoto(const char *cap, int col, int row);
void wasm_tputs(const char *str, int affcnt, int (*outc)(int));
#define tgetent wasm_tgetent
#define tgetstr wasm_tgetstr
#define tgetnum wasm_tgetnum
#define tgetflag wasm_tgetflag
#define tgoto wasm_tgoto
#define tputs wasm_tputs

/* Pause for effect; on Emscripten this yields to the browser event loop,
 * which also lets pending terminal output flush to xterm.js. */
int delay_output(int ms);

#endif /* UTW_TUI_H */
