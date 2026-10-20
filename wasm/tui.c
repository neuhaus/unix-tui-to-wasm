/* tui.c - minimal termcap backend for the xterm.js terminal.
 *
 * Extracted from restoHack's wasm port. Provides the termcap entry points
 * as fixed ANSI escape sequences: there is exactly one terminal (the
 * xterm.js canvas), so tgetent() always succeeds and tgetnum() reports the
 * geometry from UTW_COLS/UTW_ROWS (set by utw.cmake). This file compiles
 * (as harmless shims) with a native compiler too, which keeps the shim
 * logic testable without an Emscripten SDK.
 */

#include "tui.h"

#ifndef UTW_COLS
#define UTW_COLS 80
#endif
#ifndef UTW_ROWS
#define UTW_ROWS 24
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define TUI_SLEEP(ms) emscripten_sleep(ms)
#else
/* Native fallback: no browser event loop to yield to. */
#define TUI_SLEEP(ms) ((void)0)
#endif

#include <stdio.h>
#include <string.h>

int wasm_tgetent(char *bp, const char *name) {
  (void)bp;
  (void)name;
  return 1; /* exactly one terminal type: the xterm.js canvas */
}

char *wasm_tgetstr(const char *id, char **area) {
  (void)area;

  /* Capability strings as ANSI escapes. Anything else returns NULL
   * (capability absent), exactly like a minimal real termcap entry. */
  static char cm[] = "CM"; /* cursor motion: expanded by wasm_tgoto() */
  static char cl[] = "\033[H\033[2J";
  static char ho[] = "\033[H";
  static char ce[] = "\033[K";
  static char cd[] = "\033[J";
  static char up[] = "\033[A";
  static char do_[] = "\033[B";
  static char nd[] = "\033[C";
  static char bc[] = "\010";
  static char kb[] = "\177";
  static char ku[] = "\033[A";
  static char kd[] = "\033[B";
  static char kl[] = "\033[D";
  static char kr[] = "\033[C";
  static char so[] = "\033[7m";
  static char se[] = "\033[27m";

  if (strcmp(id, "cm") == 0)
    return cm;
  if (strcmp(id, "cl") == 0)
    return cl;
  if (strcmp(id, "ho") == 0)
    return ho;
  if (strcmp(id, "ce") == 0)
    return ce;
  if (strcmp(id, "cd") == 0)
    return cd;
  if (strcmp(id, "up") == 0)
    return up;
  if (strcmp(id, "do") == 0)
    return do_;
  if (strcmp(id, "nd") == 0)
    return nd;
  if (strcmp(id, "bc") == 0)
    return bc;
  if (strcmp(id, "kb") == 0)
    return kb;
  if (strcmp(id, "ku") == 0)
    return ku;
  if (strcmp(id, "kd") == 0)
    return kd;
  if (strcmp(id, "kl") == 0)
    return kl;
  if (strcmp(id, "kr") == 0)
    return kr;
  if (strcmp(id, "so") == 0)
    return so;
  if (strcmp(id, "se") == 0)
    return se;
  return NULL; /* pc, xd, ...: absent, caller has fallbacks */
}

int wasm_tgetnum(const char *id) {
  if (strcmp(id, "co") == 0)
    return UTW_COLS;
  if (strcmp(id, "li") == 0)
    return UTW_ROWS;
  return -1; /* sg, ...: absent */
}

int wasm_tgetflag(const char *id) {
  /* xterm's backspace key works; NP, nx and os are absent. */
  return strcmp(id, "bs") == 0;
}

char *wasm_tgoto(const char *cap, int col, int row) {
  static char buf[32];
  (void)cap; /* only ever called for CM */
  /* tgoto takes 0-based columns/rows; ANSI CUP wants 1-based. */
  (void)snprintf(buf, sizeof(buf), "\033[%d;%dH", row + 1, col + 1);
  return buf;
}

void wasm_tputs(const char *str, int affcnt, int (*outc)(int)) {
  (void)affcnt; /* no padding accounting: our strings carry no $<...> */
  if (str == NULL)
    return;
  while (*str)
    outc((unsigned char)*str++);
}

int delay_output(int ms) {
  if (ms > 0)
    TUI_SLEEP((unsigned int)ms);
  return 0;
}

#ifdef __EMSCRIPTEN__
/* Single-user process identity. Emscripten's libc has no passwd database
 * (getpwuid() returns NULL) and rejects every setuid() call, which breaks
 * programs written for setuid games. The browser has exactly one user and
 * no privileges to drop, so these shims replace the libc functions in the
 * link: the name and home directory come from $USER and $HOME (pre-js can
 * set them), and setuid() succeeds without doing anything. */
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>

struct passwd *getpwuid(uid_t uid) {
  static struct passwd pw;
  const char *name = getenv("USER");
  const char *home = getenv("HOME");

  pw.pw_name = (char *)(name && *name ? name : "player");
  pw.pw_passwd = (char *)"*";
  pw.pw_uid = uid;
  pw.pw_gid = (gid_t)uid;
  pw.pw_gecos = (char *)"";
  pw.pw_dir = (char *)(home && *home ? home : "/");
  pw.pw_shell = (char *)"/bin/sh";
  return &pw;
}

int setuid(uid_t uid) {
  (void)uid;
  return 0;
}
#endif /* __EMSCRIPTEN__ */
