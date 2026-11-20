/**
 * \file curses.c
 * \brief Minimal curses implementation for the browser (WebAssembly) port.
 *
 * Renders a curses-like screen into the xterm.js terminal set up by
 * unix-tui-to-wasm: the screen is kept in memory, diffs are emitted as
 * ANSI escape sequences on the standard output, and input is read from
 * the xterm-pty bridge with poll() based timeouts.
 *
 * The implementation covers what main-gcu.c needs: windows, a per-cell
 * character/attribute buffer, color pairs (including the 256 color
 * indices main-gcu.c derives from Angband's color table), and blocking,
 * non-blocking and half-delay getch().
 */

#include "curses.h"

#ifdef __EMSCRIPTEN__

#include "tui.h" /* delay_output(): yields to the browser event loop */

#include <errno.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#ifndef UTW_COLS
#define UTW_COLS 80
#endif
#ifndef UTW_ROWS
#define UTW_ROWS 24
#endif

/* One screen cell. */
struct wasm_curses_cell {
	wchar_t ch;
	chtype attr;
};

WINDOW *stdscr;
int LINES = UTW_ROWS;
int COLS = UTW_COLS;
int COLORS = 256;
int COLOR_PAIRS = 256;

/* The composited screen and what the terminal currently shows. */
static wasm_curses_cell *screen_cells;
static wasm_curses_cell *shadow_cells;

static short pair_fg[256];
static short pair_bg[256];

static struct termios saved_termios;
static struct termios game_termios;
static bool termios_saved;

static bool cursor_visible = true;
static bool have_pen;		/* an SGR sequence is active */
static chtype pen_attr;

/* Ungetchar stack. */
static unsigned char pushback[32];
static int npushback;

/* ---------------------------------------------------------------- output */

static char outbuf[65536];
static size_t outlen;

static void out_flush(void)
{
	ssize_t n;

	while (outlen > 0) {
		n = write(STDOUT_FILENO, outbuf, outlen);
		if (n < 0) {
			if (errno == EINTR) continue;
			outlen = 0;
			return;
		}
		outlen -= (size_t)n;
		if (outlen > 0)
			memmove(outbuf, outbuf + n, outlen);
	}
}

static void out_bytes(const char *s, size_t n)
{
	if (n > sizeof(outbuf)) {
		out_flush();
		while (n > 0) {
			ssize_t w = write(STDOUT_FILENO, s, n);
			if (w <= 0) {
				if (w < 0 && errno == EINTR) continue;
				return;
			}
			s += w;
			n -= (size_t)w;
		}
		return;
	}
	if (outlen + n > sizeof(outbuf))
		out_flush();
	memcpy(outbuf + outlen, s, n);
	outlen += n;
}

static void out_str(const char *s)
{
	out_bytes(s, strlen(s));
}

static void out_fmt(const char *fmt, ...)
{
	char buf[64];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n > 0)
		out_bytes(buf, (n < (int)sizeof(buf)) ? (size_t)n
			: sizeof(buf) - 1);
}

static int utf8_encode(wchar_t wc, char *out)
{
	uint32_t c = (uint32_t)wc;

	if (c == 0) c = ' ';
	if (c < 0x80) {
		out[0] = (char)c;
		return 1;
	}
	if (c < 0x800) {
		out[0] = (char)(0xc0 | (c >> 6));
		out[1] = (char)(0x80 | (c & 0x3f));
		return 2;
	}
	if (c < 0x10000) {
		if (c >= 0xd800 && c <= 0xdfff) c = 0xfffd;
		out[0] = (char)(0xe0 | (c >> 12));
		out[1] = (char)(0x80 | ((c >> 6) & 0x3f));
		out[2] = (char)(0x80 | (c & 0x3f));
		return 3;
	}
	out[0] = (char)(0xf0 | (c >> 18));
	out[1] = (char)(0x80 | ((c >> 12) & 0x3f));
	out[2] = (char)(0x80 | ((c >> 6) & 0x3f));
	out[3] = (char)(0x80 | (c & 0x3f));
	return 4;
}

/* Emit the SGR attributes (and colors) for a cell. */
static void out_attr(chtype attr)
{
	int pair = PAIR_NUMBER(attr);
	int fg = 7, bg = 0;

	if (pair >= 0 && pair < COLOR_PAIRS && pair_fg[pair] != -2) {
		fg = pair_fg[pair];
		bg = pair_bg[pair];
	}

	out_str("\033[0");
	if (attr & A_BOLD) out_str(";1");
	if (attr & A_DIM) out_str(";2");
	if (attr & A_UNDERLINE) out_str(";4");
	if (attr & A_BLINK) out_str(";5");
	if (attr & A_REVERSE) out_str(";7");
	out_str("m");

	if (fg < 0) {
		out_str("\033[39m");
	} else if (fg < 8) {
		out_fmt("\033[%dm", 30 + fg);
	} else if (fg < 16) {
		out_fmt("\033[%dm", 90 + fg - 8);
	} else {
		out_fmt("\033[38;5;%dm", fg);
	}

	if (bg < 0) {
		out_str("\033[49m");
	} else if (bg < 8) {
		out_fmt("\033[%dm", 40 + bg);
	} else if (bg < 16) {
		out_fmt("\033[%dm", 100 + bg - 8);
	} else {
		out_fmt("\033[48;5;%dm", bg);
	}

	have_pen = true;
	pen_attr = attr;
}

static bool cell_same(const wasm_curses_cell *a, const wasm_curses_cell *b)
{
	return a->ch == b->ch && a->attr == b->attr;
}

static wasm_curses_cell blank_cell(chtype bkgd)
{
	wasm_curses_cell c;
	wchar_t wc = (wchar_t)(bkgd & A_CHARTEXT);

	c.ch = wc ? wc : L' ';
	c.attr = bkgd & ~A_CHARTEXT;
	return c;
}

/* Move the hardware cursor to the window's cursor and mark the output
 * pending.  Used after compositing and drawing. */
static void place_cursor(WINDOW *win)
{
	int y, x;

	if (!win || !cursor_visible) return;
	y = win->begy + win->cury;
	x = win->begx + win->curx;
	if (y < 0 || y >= LINES || x < 0 || x >= COLS) return;
	out_fmt("\033[%d;%dH", y + 1, x + 1);
}

/* Emit the cells that differ from what the terminal displays. */
static void flush_screen(WINDOW *win)
{
	int x, y;

	if (!screen_cells || !shadow_cells) return;

	for (y = 0; y < LINES; y++) {
		x = 0;
		while (x < COLS) {
			int i = y * COLS + x;

			if (cell_same(&screen_cells[i], &shadow_cells[i])) {
				x++;
				continue;
			}

			out_fmt("\033[%d;%dH", y + 1, x + 1);
			for (; x < COLS; x++, i++) {
				char u[8];
				int n;

				if (cell_same(&screen_cells[i], &shadow_cells[i]))
					break;
				if (!have_pen ||
				    screen_cells[i].attr != pen_attr)
					out_attr(screen_cells[i].attr);
				n = utf8_encode(screen_cells[i].ch, u);
				out_bytes(u, (size_t)n);
				shadow_cells[i] = screen_cells[i];
			}
		}
	}

	place_cursor(win);
	out_flush();
}

/* ------------------------------------------------------------ attributes */

int wattrset(WINDOW *win, chtype attrs)
{
	if (!win) return ERR;
	win->attr = attrs & ~A_CHARTEXT;
	return OK;
}

int wbkgdset(WINDOW *win, chtype ch)
{
	if (!win) return ERR;
	win->bkgd = ch;
	return OK;
}

chtype getbkgd(WINDOW *win)
{
	return win ? win->bkgd : 0;
}

int wbkgd(WINDOW *win, chtype ch)
{
	int i, n;

	if (!win) return ERR;
	win->bkgd = ch;
	n = win->lines * win->cols;
	for (i = 0; i < n; i++)
		win->cells[i].attr = ch & ~A_CHARTEXT;
	return OK;
}

/* ----------------------------------------------------------------- color */

int start_color(void) { return OK; }
bool has_colors(void) { return true; }
bool can_change_color(void) { return false; }

int init_pair(short pair, short fg, short bg)
{
	if (pair < 0 || pair >= COLOR_PAIRS) return ERR;
	pair_fg[pair] = fg;
	pair_bg[pair] = bg;
	return OK;
}

int init_color(short color, short r, short g, short b)
{
	(void)color;
	(void)r;
	(void)g;
	(void)b;
	return OK;
}

int use_default_colors(void)
{
	int i;

	for (i = 0; i < 256; i++) {
		pair_fg[i] = -1;
		pair_bg[i] = -1;
	}
	return OK;
}

/* ---------------------------------------------------------------- windows */

WINDOW *newwin(int nlines, int ncols, int begin_y, int begin_x)
{
	WINDOW *win;
	int i, n;

	if (nlines <= 0) nlines = LINES - begin_y;
	if (ncols <= 0) ncols = COLS - begin_x;
	if (nlines <= 0 || ncols <= 0) return NULL;

	win = calloc(1, sizeof(*win));
	if (!win) return NULL;
	win->cells = malloc((size_t)nlines * (size_t)ncols *
		sizeof(*win->cells));
	if (!win->cells) {
		free(win);
		return NULL;
	}

	win->lines = nlines;
	win->cols = ncols;
	win->begy = begin_y;
	win->begx = begin_x;
	win->attr = A_NORMAL;
	win->bkgd = (chtype)' ';
	win->delay = -1;
	win->nodelay = false;
	win->keypad = false;

	n = nlines * ncols;
	for (i = 0; i < n; i++) {
		win->cells[i].ch = L' ';
		win->cells[i].attr = A_NORMAL;
	}
	return win;
}

int delwin(WINDOW *win)
{
	if (!win || win == stdscr) return ERR;
	free(win->cells);
	free(win);
	return OK;
}

int wresize(WINDOW *win, int lines, int cols)
{
	wasm_curses_cell *cells;
	int x, y, xmax, ymax;

	if (!win || lines <= 0 || cols <= 0) return ERR;
	cells = malloc((size_t)lines * (size_t)cols * sizeof(*cells));
	if (!cells) return ERR;

	xmax = (cols < win->cols) ? cols : win->cols;
	ymax = (lines < win->lines) ? lines : win->lines;
	for (y = 0; y < lines; y++) {
		for (x = 0; x < cols; x++) {
			wasm_curses_cell c = blank_cell(win->bkgd);

			if (y < ymax && x < xmax)
				c = win->cells[y * win->cols + x];
			cells[y * cols + x] = c;
		}
	}

	free(win->cells);
	win->cells = cells;
	win->lines = lines;
	win->cols = cols;
	if (win->cury >= lines) win->cury = lines - 1;
	if (win->curx >= cols) win->curx = cols - 1;

	/* A resized standard screen changes the physical screen size. */
	if (win == stdscr) {
		wasm_curses_cell *ns, *nsh;
		int i, n = lines * cols;

		ns = realloc(screen_cells, (size_t)n * sizeof(*ns));
		nsh = realloc(shadow_cells, (size_t)n * sizeof(*nsh));
		if (ns) screen_cells = ns;
		if (nsh) shadow_cells = nsh;
		for (i = 0; i < n; i++) {
			screen_cells[i] = blank_cell(win->bkgd);
			shadow_cells[i].ch = 0;
			shadow_cells[i].attr = (chtype)~0u;
		}
		LINES = lines;
		COLS = cols;
	}
	return OK;
}

int wmove(WINDOW *win, int y, int x)
{
	if (!win || y < 0 || x < 0 || y >= win->lines || x >= win->cols)
		return ERR;
	win->cury = y;
	win->curx = x;
	return OK;
}

int wclear(WINDOW *win)
{
	return werase(win);
}

int clear(void)
{
	return werase(stdscr);
}

int werase(WINDOW *win)
{
	int i, n;

	if (!win) return ERR;
	n = win->lines * win->cols;
	for (i = 0; i < n; i++)
		win->cells[i] = blank_cell(win->bkgd);
	win->cury = 0;
	win->curx = 0;
	return OK;
}

int touchwin(WINDOW *win)
{
	(void)win;
	return OK;
}

int whline(WINDOW *win, chtype ch, int n)
{
	int x, end;
	wchar_t wc = (wchar_t)(ch & A_CHARTEXT);

	if (!win) return ERR;
	if (!wc) wc = L' ';
	if (n < 0) n = win->cols - win->curx;
	end = win->curx + n;
	if (end > win->cols) end = win->cols;
	for (x = win->curx; x < end; x++) {
		win->cells[win->cury * win->cols + x].ch = wc;
		win->cells[win->cury * win->cols + x].attr =
			win->attr & ~A_CHARTEXT;
	}
	return OK;
}

int wclrtoeol(WINDOW *win)
{
	int x;

	if (!win) return ERR;
	for (x = win->curx; x < win->cols; x++)
		win->cells[win->cury * win->cols + x] = blank_cell(win->bkgd);
	return OK;
}

/* Advance the window's cursor, wrapping and scrolling like curses. */
static void advance(WINDOW *win)
{
	if (++win->curx < win->cols) return;
	win->curx = 0;
	if (++win->cury < win->lines) return;

	/* Scroll one line. */
	memmove(win->cells, win->cells + win->cols,
		(size_t)(win->lines - 1) * win->cols * sizeof(*win->cells));
	{
		int x;
		for (x = 0; x < win->cols; x++)
			win->cells[(win->lines - 1) * win->cols + x] =
				blank_cell(win->bkgd);
	}
	win->cury = win->lines - 1;
}

int waddnwstr(WINDOW *win, const wchar_t *wstr, int n)
{
	int i;

	if (!win || !wstr) return ERR;
	for (i = 0; i < n && wstr[i] != L'\0'; i++) {
		wchar_t wc = wstr[i];

		if (wc == L'\n') {
			win->curx = 0;
			if (++win->cury >= win->lines) {
				win->cury = win->lines - 1;
				advance(win);
				win->curx = 0;
			}
			continue;
		}
		if (wc == L'\r') {
			win->curx = 0;
			continue;
		}
		if (wc == L'\t') {
			int stop = (win->curx + 8) & ~7;
			while (win->curx < stop && win->curx < win->cols) {
				win->cells[win->cury * win->cols + win->curx].ch =
					L' ';
				advance(win);
			}
			continue;
		}
		win->cells[win->cury * win->cols + win->curx].ch = wc;
		win->cells[win->cury * win->cols + win->curx].attr =
			win->attr & ~A_CHARTEXT;
		advance(win);
	}
	return OK;
}

int mvwaddnwstr(WINDOW *win, int y, int x, const wchar_t *wstr, int n)
{
	if (wmove(win, y, x) == ERR) return ERR;
	return waddnwstr(win, wstr, n);
}

/* -------------------------------------------------------------- refreshing */

int wrefresh(WINDOW *win)
{
	int x, y, xmax, ymax;

	if (!win || !screen_cells) return ERR;

	xmax = win->cols;
	if (win->begx + xmax > COLS) xmax = COLS - win->begx;
	ymax = win->lines;
	if (win->begy + ymax > LINES) ymax = LINES - win->begy;

	for (y = 0; y < ymax; y++) {
		for (x = 0; x < xmax; x++) {
			screen_cells[(win->begy + y) * COLS + win->begx + x] =
				win->cells[y * win->cols + x];
		}
	}

	flush_screen(win);
	return OK;
}

int refresh(void)
{
	return wrefresh(stdscr);
}

/* ------------------------------------------------------------- terminal */

WINDOW *initscr(void)
{
	int i, n;
	struct winsize ws;

	if (stdscr) return stdscr;

	/* The PTY knows the real terminal geometry (the page sets it from the
	 * #cols=...&rows=... fragment); fall back to the compiled defaults. */
	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0 &&
	    ws.ws_row > 0 && ws.ws_col > 0) {
		LINES = ws.ws_row;
		COLS = ws.ws_col;
	}

	stdscr = newwin(LINES, COLS, 0, 0);
	if (!stdscr) return NULL;

	n = LINES * COLS;
	screen_cells = malloc((size_t)n * sizeof(*screen_cells));
	shadow_cells = malloc((size_t)n * sizeof(*shadow_cells));
	if (!screen_cells || !shadow_cells) {
		free(screen_cells);
		free(shadow_cells);
		screen_cells = shadow_cells = NULL;
		delwin(stdscr);
		stdscr = NULL;
		return NULL;
	}

	stdscr->begy = 0;
	stdscr->begx = 0;
	stdscr->lines = LINES;
	stdscr->cols = COLS;
	for (i = 0; i < n; i++) {
		screen_cells[i] = blank_cell(0);
		shadow_cells[i].ch = 0;
		shadow_cells[i].attr = (chtype)~0u;
	}

	if (tcgetattr(STDIN_FILENO, &saved_termios) == 0) {
		game_termios = saved_termios;
		termios_saved = true;
	}

	/* No autowrap (Angband avoids the bottom right corner but runs of
	 * text should never scroll the screen) and a clean slate. */
	out_str("\033[?7l\033[0m\033[H\033[2J\033[?25h");
	out_flush();

	have_pen = false;
	cursor_visible = true;

	/* Pair 0 is white on black, like curses. */
	for (i = 0; i < 256; i++) {
		pair_fg[i] = -2;
		pair_bg[i] = -2;
	}
	pair_fg[0] = COLOR_WHITE;
	pair_bg[0] = COLOR_BLACK;

	return stdscr;
}

int endwin(void)
{
	int y, x;

	if (!stdscr) return ERR;

	out_str("\033[0m\033[?25h");
	out_flush();
	have_pen = false;

	if (termios_saved)
		tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);

	getyx(stdscr, y, x);
	(void)y;
	(void)x;
	return OK;
}

int mvcur(int oldrow, int oldcol, int newrow, int newcol)
{
	(void)oldrow;
	(void)oldcol;
	if (newrow < 0 || newcol < 0) return ERR;
	out_fmt("\033[%d;%dH", newrow + 1, newcol + 1);
	out_flush();
	return OK;
}

int curs_set(int visibility)
{
	int old = cursor_visible;

	cursor_visible = (visibility != 0);
	out_str(cursor_visible ? "\033[?25h" : "\033[?25l");
	out_flush();
	return old;
}

int beep(void)
{
	out_str("\a");
	out_flush();
	return OK;
}

int flash(void)
{
	return OK;
}

/* ------------------------------------------------------------------ input */

static void apply_termios(void)
{
	if (termios_saved)
		tcsetattr(STDIN_FILENO, TCSANOW, &game_termios);
}

int raw(void)
{
	if (termios_saved) {
		game_termios.c_lflag &= ~(ICANON | ECHO | IEXTEN);
		game_termios.c_lflag |= ISIG;
		game_termios.c_iflag &= ~(IXON | ICRNL | BRKINT);
		game_termios.c_cc[VMIN] = 1;
		game_termios.c_cc[VTIME] = 0;
		apply_termios();
	}
	return OK;
}

int cbreak(void)
{
	if (termios_saved) {
		game_termios.c_lflag &= ~ICANON;
		game_termios.c_lflag |= ISIG;
		game_termios.c_cc[VMIN] = 1;
		game_termios.c_cc[VTIME] = 0;
		apply_termios();
	}
	if (stdscr) {
		stdscr->nodelay = false;
		stdscr->delay = -1;
	}
	return OK;
}

int nocbreak(void)
{
	if (termios_saved) {
		game_termios.c_lflag |= ICANON;
		apply_termios();
	}
	return OK;
}

int echo(void)
{
	if (termios_saved) {
		game_termios.c_lflag |= ECHO;
		apply_termios();
	}
	return OK;
}

int noecho(void)
{
	if (termios_saved) {
		game_termios.c_lflag &= ~ECHO;
		apply_termios();
	}
	return OK;
}

int nl(void)
{
	if (termios_saved) {
		game_termios.c_iflag |= ICRNL;
		apply_termios();
	}
	return OK;
}

int nonl(void)
{
	if (termios_saved) {
		game_termios.c_iflag &= ~ICRNL;
		apply_termios();
	}
	return OK;
}

int nodelay(WINDOW *win, bool flag)
{
	if (!win) return ERR;
	win->nodelay = flag;
	if (flag) win->delay = -1;
	return OK;
}

int keypad(WINDOW *win, bool flag)
{
	if (!win) return ERR;
	win->keypad = flag;
	return OK;
}

int halfdelay(int tenths)
{
	if (!stdscr) return ERR;
	stdscr->nodelay = false;
	stdscr->delay = tenths * 100;
	return OK;
}

int ungetch(int ch)
{
	if (npushback >= (int)sizeof(pushback)) return ERR;
	pushback[npushback++] = (unsigned char)ch;
	return OK;
}

/* Read one byte, optionally waiting at most timeout milliseconds (a
 * negative timeout blocks).  Returns -1 on timeout or end of input. */
static int read_byte(int timeout)
{
	struct pollfd pfd;
	unsigned char c;
	ssize_t n;
	int r;

	if (timeout >= 0) {
		pfd.fd = STDIN_FILENO;
		pfd.events = POLLIN;
		pfd.revents = 0;
		do {
			r = poll(&pfd, 1, timeout);
		} while (r < 0 && errno == EINTR);
		if (r <= 0) return -1;
	}

	do {
		n = read(STDIN_FILENO, &c, 1);
	} while (n < 0 && errno == EINTR);
	if (n != 1) return -1;
	return (int)c;
}

/* Translate an xterm escape sequence starting after ESC.  Consumed bytes
 * are pushed back when the sequence is not understood, so Angband's own
 * ESC handling can see them. */
static int read_escape(void)
{
	int a, b, code, i;
	char seq[16];
	int nseq = 0;

	a = read_byte(20);
	if (a < 0) return 27;
	seq[nseq++] = (char)a;

	if (a == '[') {
		b = read_byte(20);
		if (b < 0) goto unknown;
		seq[nseq++] = (char)b;

		switch (b) {
		case 'A': return KEY_UP;
		case 'B': return KEY_DOWN;
		case 'C': return KEY_RIGHT;
		case 'D': return KEY_LEFT;
		case 'H': return KEY_HOME;
		case 'F': return KEY_END;
		case 'Z': return '\t';
		default: break;
		}

		if (b < '0' || b > '9') goto unknown;

		code = b - '0';
		while (nseq < (int)sizeof(seq) - 1) {
			b = read_byte(20);
			if (b < 0) goto unknown;
			seq[nseq++] = (char)b;
			if (b == '~') break;
			if (b < '0' || b > '9') goto unknown;
			code = code * 10 + (b - '0');
		}
		if (nseq == 0 || seq[nseq - 1] != '~') goto unknown;

		switch (code) {
		case 1: return KEY_HOME;
		case 2: return KEY_IC;
		case 3: return KEY_DC;
		case 4: return KEY_END;
		case 5: return KEY_PPAGE;
		case 6: return KEY_NPAGE;
		case 7: return KEY_HOME;
		case 8: return KEY_END;
		default: goto unknown;
		}
	}

	if (a == 'O') {
		b = read_byte(20);
		if (b < 0) goto unknown;
		seq[nseq++] = (char)b;
		switch (b) {
		case 'A': return KEY_UP;
		case 'B': return KEY_DOWN;
		case 'C': return KEY_RIGHT;
		case 'D': return KEY_LEFT;
		case 'H': return KEY_HOME;
		case 'F': return KEY_END;
		case 'P': return KEY_F(1);
		case 'Q': return KEY_F(2);
		case 'R': return KEY_F(3);
		case 'S': return KEY_F(4);
		default: goto unknown;
		}
	}

unknown:
	for (i = nseq - 1; i >= 0; i--)
		ungetch((unsigned char)seq[i]);
	return 27;
}

int getch(void)
{
	int c, timeout;

	if (!stdscr) return ERR;
	if (npushback > 0) return pushback[--npushback];

	timeout = stdscr->nodelay ? 0 : stdscr->delay;
	c = read_byte(timeout);
	if (c < 0) return ERR;

	if (c == 0x7f && stdscr->keypad)
		return KEY_BACKSPACE;
	if (c == 27 && stdscr->keypad)
		return read_escape();
	return c;
}

/* usleep() from the game loop yields to the browser instead of blocking
 * it, so queued output reaches xterm.js. */
int usleep(useconds_t usec)
{
	delay_output((int)(usec / 1000));
	return 0;
}

#endif /* __EMSCRIPTEN__ */
