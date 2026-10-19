# unix-tui-to-wasm

A small template for running historical Unix text programs (termcap) in the
browser via Emscripten and xterm-pty: Emscripten build setup, a termcap shim,
the xterm-pty bridge and an optional persistent filesystem (IDBFS), ready to
copy into another project. A mini demo (`src/demo.c`) serves as a smoke test.

## Run the demo

```sh
# Install Emscripten once (pinned version: wasm/emsdk-version)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install 3.1.47 && ./emsdk activate 3.1.47
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
   set(UTW_PRELOAD_DIR "${CMAKE_SOURCE_DIR}/static") # optional data files
   include(${CMAKE_SOURCE_DIR}/wasm/utw.cmake)
   utw_enable(myprogram)
   ```

3. Route termcap calls to the shims:

   ```c
   #ifdef __EMSCRIPTEN__
   #include "tui.h"
   #endif
   ```

   The `<curses.h>`/`<termcap.h>` stubs live in `wasm/stub-include/`.
4. Run `./wasm/build-wasm.sh` (it uses your `CMakeLists.txt`); the bundle
   ends up in `build/web`.

More options: `UTW_ROWS`, `UTW_PERSIST_DIR`, `UTW_WEB_DIR`, `UTW_WEB_FILES`,
`UTW_PTY_JS`, `UTW_PAGE`, `UTW_PRE_JS_EXTRA`; script variables `WEB_DIR`,
`OBJ_DIR`, `ALLOW_UNTESTED_EMSDK=1`. The page can pass a `displayName`
callback to `createStorageManager()` to map stored file names for display.

## Limits

- Only the termcap subset implemented in `wasm/tui.c` (no curses, no terminfo).
- Fixed terminal; the geometry comes from `UTW_COLS`/`UTW_ROWS`.
- No `fork`/`exec`: external pagers or editors need a replacement.
- No `-fstack-protector` with Emscripten 3.1.47 (the runtime is missing).

The infrastructure was extracted from the wasm port of restoHack.

## Licence

BSD-3-Clause, see [LICENCE](LICENCE).
