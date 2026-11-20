# unix-tui-to-wasm

A small template for running historical Unix text programs (termcap or
curses) in the browser via Emscripten and xterm-pty: Emscripten build setup, a
termcap shim, an optional minimal curses layer, the xterm-pty bridge and an
optional persistent filesystem (IDBFS), ready to copy into another project. A
mini demo (`src/demo.c`) serves as a smoke test.
See [FEATURES.md](FEATURES.md) for a list of everything it provides.

## Run the demo

```sh
# Install Emscripten once (pinned version: wasm/emsdk-version)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install 7.0.0 && ./emsdk activate 7.0.0
source ./emsdk_env.sh

./wasm/build-wasm.sh        # fetches xterm.js/xterm-pty (checksum-verified)
cd build/web && python3 -m http.server 8000   # -> http://localhost:8000/
```

Serve over HTTP (ES modules + IndexedDB), not via `file://`.
Native sanity check without Emscripten: `cmake -B build && cmake --build build`,
then `./build/demo`.

## Porting your own program

1. Copy `wasm/` next to your sources.
2. In CMake: set the options and register your target:

   ```cmake
   set(UTW_TITLE "my program")
   set(UTW_COLS 80)              # optional, default 80x24
   set(UTW_PERSIST ON)           # optional: IDBFS mounted at /data
   set(UTW_CURSES pdcurses)      # optional: full curses via PDCursesMod
   set(UTW_PRELOAD_DIR "${CMAKE_SOURCE_DIR}/static") # optional data files
   include(${CMAKE_SOURCE_DIR}/wasm/utw.cmake)
   utw_enable(myprogram)
   ```

   `UTW_CURSES` also accepts `ON` for the minimal bundled shim and `OFF`
   (the default) for termcap-only programs. `UTW_TERMCAP` (default `ON`)
   controls the termcap shim independently.

3. Route termcap calls to the shims:

   ```c
   #ifdef __EMSCRIPTEN__
   #include "tui.h"
   #endif
   ```

   The `<curses.h>`/`<termcap.h>` stubs live in `wasm/stub-include/`.
   `tui.h` also declares `delay_output()` for animation timing (it yields
   to the browser event loop); ports with their own fallback should treat
   it as already provided in the Emscripten build.
   Emscripten's libc has no passwd database and refuses `setuid()`, so
   `compat.c` replaces `getpwuid()`/`getpwnam()` (name from `$USER`, home
   from `$HOME`) and `setuid()` (always succeeds) with single-user versions.
   Programs that look themselves up or drop privileges need no `#ifdef`.
   Programs already written against curses can set `UTW_CURSES ON` for the
   minimal bundled shim (`wasm/curses/`: windows, color pairs, wide character
   output, timed input) or `UTW_CURSES pdcurses` for the full PDCursesMod VT
   port (pads, panels, menus, mouse, 256 colors) from the
   `wasm/PDCursesMod` submodule.
4. Run `./wasm/build-wasm.sh` (it uses your `CMakeLists.txt` and fetches the
   `wasm/PDCursesMod` submodule if the tree has git metadata); the bundle
   ends up in `build/web`.

More options: `UTW_ROWS`, `UTW_PERSIST_DIR`, `UTW_WEB_DIR`, `UTW_WEB_FILES`,
`UTW_PTY_JS`, `UTW_PAGE`, `UTW_PRE_JS_EXTRA`; script variables `WEB_DIR`,
`OBJ_DIR`, `ALLOW_UNTESTED_EMSDK=1`.

The geometry can also be chosen per load with a URL fragment, e.g.
`index.html#cols=120&rows=40`.  The page resizes the terminal and the PTY
before the program starts, and the shims read the size from the PTY, so no
rebuild is needed.  Reload the page to change it; the geometry is fixed for
the lifetime of the program.  `UTW_COLS`/`UTW_ROWS` remain the defaults. The page can pass a `displayName`
callback to `createStorageManager()` to map stored file names for display.

## Limits

- Only the termcap subset implemented in `wasm/tui.c` (no terminfo), plus a
  curses backend of your choice: the minimal `wasm/curses/` shim or
  PDCursesMod's VT port. The minimal shim has no pads, menus, forms or mouse
  support, and no line-drawing (ACS) characters; PDCurses' terminfo/termcap
  functions are stubs, so mixed termcap code still needs `tui.c`.
- Fixed terminal; the geometry defaults to `UTW_COLS`/`UTW_ROWS` and can be
  overridden for a load with `#cols=N&rows=M`.  It does not change while the
  program runs.
- No `fork`/`exec`: external pagers or editors need a replacement.
- IDBFS restores files with new inode numbers, so `stat().st_ino` cannot be used
  to recognize a file across page reloads. Validate saved files by content or a
  format-specific token instead.
- `-fstack-protector` was unavailable with Emscripten 3.1.47. The pinned
  7.0.0 SDK is used without that flag.

The infrastructure was extracted from the wasm port of restoHack.

## Licence

BSD-3-Clause, see [LICENCE](LICENCE).
