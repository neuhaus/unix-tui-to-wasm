# What unix-tui-to-wasm provides

A checklist of everything in the template, grouped by where it lives. See
[README.md](README.md) for how to use it.

## Build (`wasm/utw.cmake`, `wasm/build-wasm.sh`)

- `utw_enable(<target>)` for CMake projects. It adds the shim sources, the
  stub include path and the Emscripten link options, and creates a
  `<target>_web` target that stages the servable bundle.
- Link options set for you: Asyncify, ES6 module output, growable memory,
  exit-runtime, the `FS` runtime export and IDBFS.
- Options: `UTW_TITLE`, `UTW_COLS`, `UTW_ROWS`, `UTW_PERSIST`,
  `UTW_PERSIST_DIR`, `UTW_PRELOAD_DIR`, `UTW_WEB_DIR`.
- Hooks: `UTW_PAGE` (page template), `UTW_WEB_FILES` (extra bundle files),
  `UTW_PTY_JS` (terminal glue), `UTW_PRE_JS_EXTRA` (extra pre-js).
- `build-wasm.sh` builds and stages the bundle:
  - checks the active Emscripten against the pinned version in
    `wasm/emsdk-version` (`ALLOW_UNTESTED_EMSDK=1` overrides);
  - downloads xterm.js, xterm.css and xterm-pty, verified by SHA-256, so
    the finished page works offline;
  - configurable with `ROOT`, `WEB_DIR` and `OBJ_DIR`.
- Pinned versions: Emscripten 6.0.13, `@xterm/xterm` 5.5.0, `xterm-pty` 0.12.0.

## C shims (`wasm/tui.c`, `wasm/tui.h`, `wasm/stub-include/`)

- Termcap: `tgetent`, `tgetstr`, `tgetnum`, `tgetflag`, `tgoto` and `tputs`,
  redirected by `tui.h` to ANSI-escape versions for one fixed xterm.js
  terminal.
  - String capabilities: `cm cl ho ce cd up do nd bc kb ku kd kl kr so se`.
  - Numbers and flags: `co` and `li` from `UTW_COLS`/`UTW_ROWS`, and `bs`.
- `delay_output(ms)`: a pause that yields to the browser, so pending output
  reaches the screen.
- `getpwuid()`: a single-user answer built from `$USER` and `$HOME`
  (Emscripten's libc returns `NULL`). Emscripten build only.
- `setuid()`: always succeeds (Emscripten's libc always fails). Emscripten
  build only.
- Empty `curses.h` and `termcap.h` stubs, on the include path for the
  Emscripten build, so unchanged `#include`s still resolve.

## Browser page (`wasm/index.html.in`, `wasm/pre.js`, `wasm/storage.js`)

- Generated page with an xterm.js terminal sized by `UTW_COLS`/`UTW_ROWS`, a
  title, a stored-files line and a status line.
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
