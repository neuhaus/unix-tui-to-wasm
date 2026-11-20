# What unix-tui-to-wasm provides

A checklist of everything in the template, grouped by where it lives. See
[README.md](README.md) for how to use it.

## Build (`wasm/utw.cmake`, `wasm/build-wasm.sh`)

- `utw_enable(<target>)` for CMake projects. It adds the runtime sources
  (`compat.c`, the termcap shim unless disabled, and the selected curses
  backend), the stub include path and the Emscripten link options, and creates
  a `<target>_web` target that stages the servable bundle.
- Link options set for you: Asyncify, ES6 module output, growable memory,
  exit-runtime, the `FS` runtime export and IDBFS.
- Options: `UTW_TITLE`, `UTW_COLS`, `UTW_ROWS`, `UTW_PERSIST`,
  `UTW_PERSIST_DIR`, `UTW_CURSES` (`ON`/`OFF`/`pdcurses`), `UTW_TERMCAP`,
  `UTW_MINIFY`, `UTW_LZ4`, `UTW_PDCURSES_DIR`, `UTW_PRELOAD_DIR`,
  `UTW_WEB_DIR`.
- Hooks: `UTW_PAGE` (page template), `UTW_WEB_FILES` (extra bundle files),
  `UTW_PTY_JS` (terminal glue), `UTW_PRE_JS_EXTRA` (extra pre-js).
- `build-wasm.sh` builds and stages the bundle:
  - checks the active Emscripten against the pinned version in
    `emsdk-version` (`ALLOW_UNTESTED_EMSDK=1` overrides);
  - initializes the git submodules next to it, including `PDCursesMod`, when
    the tree has git metadata;
  - downloads xterm.js, xterm.css and xterm-pty, verified by SHA-256, so
    the finished page works offline;
  - builds with `MinSizeRel` and minified JS by default (`BUILD_TYPE` and
    `UTW_MINIFY` change that), and can LZ4-compress the preloaded data
    (`UTW_LZ4`);
  - configurable with `ROOT`, `WEB_DIR`, `OBJ_DIR` and `BUILD_TYPE`.
- Pinned versions: Emscripten 7.0.0, `@xterm/xterm` 5.5.0, `xterm-pty` 0.12.0,
  and the PDCursesMod commit recorded by the `wasm/PDCursesMod` submodule.
- `wasm/patch-pty.py`: an idempotent patch applied to the checksum-verified
  xterm-pty glue so version 0.12.0 keeps working on Emscripten 7, which no
  longer defines the `$TTY__deps` metadata the glue edits.

## C shims (`wasm/tui.c`, `wasm/compat.c`, `wasm/tui.h`, `wasm/stub-include/`)

- Termcap: `tgetent`, `tgetstr`, `tgetnum`, `tgetflag`, `tgoto` and `tputs`,
  redirected by `tui.h` to ANSI-escape versions for one fixed xterm.js
  terminal.
  - String capabilities: `cm cl ho ce cd up do nd bc kb ku kd kl kr so se`.
  - Numbers and flags: `co` and `li` from the PTY window size (defaulting to
    `UTW_COLS`/`UTW_ROWS`), and `bs`.
- `delay_output(ms)`: a pause that yields to the browser, so pending output
  reaches the screen. Left out when the curses backend provides its own
  (`UTW_CURSES=pdcurses` defines `UTW_NO_DELAY_OUTPUT`).
- `getpwuid()` and `getpwnam()` (`wasm/compat.c`): a single-user answer built
  from `$USER` and `$HOME` (Emscripten's libc returns `NULL`). Emscripten
  build only.
- `setuid()` (`wasm/compat.c`): always succeeds (Emscripten's libc always
  fails). Emscripten build only.
- `fork()` and `execve()` (`wasm/compat.c`): fail with `ENOSYS`, like
  Emscripten's own stubs. Defining them keeps Emscripten's libc stub object
  out of the link; that object also defines `getpwuid()`/`getpwnam()` and
  would otherwise collide with the shims above. Emscripten build only.
- Empty `curses.h` and `termcap.h` fallbacks in `wasm/stub-include/`, on the
  include path for the Emscripten build so unchanged `#include`s resolve.
  A curses backend puts its real header in front of them.

## Curses shim (`wasm/curses/`, enabled with `UTW_CURSES=ON`)

A minimal curses implementation rendered with the same ANSI escapes and
xterm-pty input as the termcap shim. It covers what Angband's GCU front end
needs and nothing more:

- Windows (`newwin`, `delwin`, `wresize`, `wmove`, `wclear`, `werase`,
  `wbkgd`, `whline`, `wclrtoeol`, `wattrset`), with wide character output
  (`mvwaddnwstr`).  `initscr` takes the screen size from the PTY
  (`TIOCGWINSZ`), so a `#cols=N&rows=M` fragment works without rebuilding.
- Colors: `start_color`, `init_pair`, `init_color`, `use_default_colors`,
  256 colors and color pairs, plus `A_BOLD`, `A_REVERSE`, `A_BLINK`, `A_DIM`
  and `A_UNDERLINE`.
- Input: `getch`, `ungetch`, `nodelay`, `halfdelay`, `keypad` (arrow and
  navigation key escape sequences) and the usual `cbreak`/`raw`/`echo`
  terminal modes, driven through xterm-pty's termios emulation.
- Only changed cells are redrawn (the screen is diffed against a shadow
  buffer), so refreshing is cheap.
- `usleep()` is routed to `delay_output()` so animations yield to the
  browser.

Not implemented: pads, menus, forms, mouse, ACS line drawing, terminfo and
`resizeterm` (the terminal geometry is fixed).

## PDCurses backend (`wasm/pdcurses.cmake`, `UTW_CURSES=pdcurses`)

The full PDCursesMod curses library instead of the minimal shim, built from
the pinned `wasm/PDCursesMod` submodule (`UTW_PDCURSES_DIR` overrides its
location):

- VT port: plain ANSI escape output for xterm.js, plus xterm-pty input.
- Wide characters (`PDC_WIDE`) and 32-bit `chtype` (`CHTYPE_32`) so color
  pairs fit in an `int`, as Angband's attribute tables expect.
- 256 colors, pads, panels, menus, mouse and the usual curses surface.
- `initscr` takes the geometry from the PTY (`TIOCGWINSZ`), so the
  `#cols=N&rows=M` fragment works without rebuilding.
- `PDC_napms()` is compiled with `usleep=utw_emscripten_usleep`
  (`wasm/pdcurses-emscripten.c`), so timed input yields to the browser event
  loop instead of freezing it. This workaround can go away once the
  corresponding Emscripten fixes are merged upstream.
- `delay_output()` comes from PDCurses; `tui.c` leaves its own copy out.
- PDCurses' terminfo/termcap entry points are stubs, so `tui.c` stays the
  termcap implementation even with this backend.

## Browser page (`wasm/index.html.in`, `wasm/pre.js`, `wasm/storage.js`)

- Generated page with an xterm.js terminal (default size from
  `UTW_COLS`/`UTW_ROWS`, overridable per load with `#cols=N&rows=M`), a
  title, a stored-files line and a status line.
- A `↔` button next to the title scales the terminal uniformly (keeping the
  character grid and aspect ratio) to fit the available window; clicking it
  again restores the normal size.  The scale is recomputed on window resize.
  If width is the limit, the stored-files and status lines stay visible under
  the terminal; if height is the limit, the terminal is enlarged as much as
  possible and those lines may move below the fold (the page scrolls).
- Terminal I/O through xterm-pty: line discipline, termios, window size and
  Ctrl+C.
- Optional persistent storage (`pre.js`): the virtual filesystem is mounted
  at `UTW_PERSIST_DIR` on IDBFS.
  - Startup waits for the load from IndexedDB.
  - Flushes run every 5 seconds and on `pagehide`, and only after a
    successful load, so a failed read can't erase stored files.
  - Falls back to in-memory files when IndexedDB is unavailable.
- `createStorageManager()` (`storage.js`):
  - lists the stored files, with a `displayName` callback to rename or hide
    them;
  - does a final flush when the program exits, with retries;
  - shows a status message afterwards.
- Read-only data files: everything in `UTW_PRELOAD_DIR` is embedded at the
  root of the virtual filesystem.

## Demo and licence

- `src/demo.c`: a small termcap program that builds natively and in the
  browser, as a smoke test.
- BSD-3-Clause (`LICENCE`).

For what the template does not cover, see "Limits" in the README.
