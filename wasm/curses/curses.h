/**
 * \file curses.h
 * \brief Minimal curses implementation for the browser (WebAssembly) port.
 *
 * Curses programs such as Angband's GCU front end (main-gcu.c) are written
 * against the curses API. Emscripten has no curses, so this header and
 * curses.c provide the small subset those programs use, rendered with ANSI
 * escapes into the xterm.js terminal managed by unix-tui-to-wasm's xterm-pty
 * bridge.
 *
 * Only a small, Angband-driven subset of curses is implemented.  This is not
 * a general curses replacement; UTW_CURSES=pdcurses builds the real thing.
 */

#ifndef UTW_WASM_CURSES_H
#define UTW_WASM_CURSES_H

#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

typedef uint32_t chtype;

typedef struct wasm_curses_window WINDOW;
typedef struct wasm_curses_cell wasm_curses_cell;

/* A curses window.  The fields are exposed because getyx() needs them. */
struct wasm_curses_window {
	int lines, cols;	/* window size */
	int begy, begx;		/* upper left corner on the screen */
	int cury, curx;		/* cursor position */
	chtype attr;		/* current drawing attribute */
	chtype bkgd;		/* background character and attribute */
	bool nodelay;		/* getch() does not wait */
	bool keypad;		/* translate escape sequences to KEY_* */
	int delay;		/* getch() timeout in ms, -1 for blocking */
	wasm_curses_cell *cells;
};

extern WINDOW *stdscr;
extern int LINES;
extern int COLS;
extern int COLORS;
extern int COLOR_PAIRS;

#define ERR (-1)
#define OK 0

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/*
 * chtype layout: bits 0-7 are the narrow character (only meaningful for
 * background cells), bits 8-15 the color pair, and the upper bits hold
 * attributes.  This matches how main-gcu.c combines ' ' with a color pair.
 */
#define A_CHARTEXT 0x000000ffu
#define A_COLOR 0x0000ff00u
#define COLOR_PAIR(n) ((((chtype)(n)) & 0xffu) << 8)
#define PAIR_NUMBER(a) ((int)(((a) & A_COLOR) >> 8))

#define A_NORMAL 0x00000000u
#define A_BOLD 0x00100000u
#define A_BRIGHT A_BOLD
#define A_DIM 0x00200000u
#define A_UNDERLINE 0x00400000u
#define A_BLINK 0x00800000u
#define A_REVERSE 0x01000000u
#define A_STANDOUT 0x02000000u

#define COLOR_BLACK 0
#define COLOR_RED 1
#define COLOR_GREEN 2
#define COLOR_YELLOW 3
#define COLOR_BLUE 4
#define COLOR_MAGENTA 5
#define COLOR_CYAN 6
#define COLOR_WHITE 7

/* Key codes.  The values follow ncurses so main-gcu.c's arithmetic, which
 * folds codes above KEY_MIN into 8 bit values, keeps working. */
#define KEY_MIN 0x101
#define KEY_DOWN 0x102
#define KEY_UP 0x103
#define KEY_LEFT 0x104
#define KEY_RIGHT 0x105
#define KEY_HOME 0x106
#define KEY_BACKSPACE 0x107
#define KEY_F0 0x108
#define KEY_F(n) (KEY_F0 + (n))
#define KEY_DC 0x14a
#define KEY_IC 0x14b
#define KEY_NPAGE 0x152
#define KEY_PPAGE 0x153
#define KEY_ENTER 0x157
#define KEY_END 0x168

#define getyx(win, y, x) \
	do { (y) = (win)->cury; (x) = (win)->curx; } while (0)

WINDOW *initscr(void);
int endwin(void);
int refresh(void);
int wrefresh(WINDOW *win);

int start_color(void);
bool has_colors(void);
bool can_change_color(void);
int init_pair(short pair, short fg, short bg);
int init_color(short color, short r, short g, short b);
int use_default_colors(void);

WINDOW *newwin(int nlines, int ncols, int begin_y, int begin_x);
int delwin(WINDOW *win);
int wresize(WINDOW *win, int lines, int cols);

int wclear(WINDOW *win);
int clear(void);
int werase(WINDOW *win);
int touchwin(WINDOW *win);
int wbkgdset(WINDOW *win, chtype ch);
int wbkgd(WINDOW *win, chtype ch);
chtype getbkgd(WINDOW *win);

int wmove(WINDOW *win, int y, int x);
int waddnwstr(WINDOW *win, const wchar_t *wstr, int n);
int mvwaddnwstr(WINDOW *win, int y, int x, const wchar_t *wstr, int n);
int whline(WINDOW *win, chtype ch, int n);
int wclrtoeol(WINDOW *win);
int wattrset(WINDOW *win, chtype attrs);

int mvcur(int oldrow, int oldcol, int newrow, int newcol);
int curs_set(int visibility);

int nodelay(WINDOW *win, bool flag);
int keypad(WINDOW *win, bool flag);
int halfdelay(int tenths);
int cbreak(void);
int nocbreak(void);
int echo(void);
int noecho(void);
int nl(void);
int nonl(void);
int raw(void);

int getch(void);
int ungetch(int ch);

int beep(void);
int flash(void);

#endif /* UTW_WASM_CURSES_H */
