# pdcurses.cmake - PDCursesMod's VT port as the curses backend.
#
#   set(UTW_CURSES pdcurses)
#   include(<this dir>/utw.cmake)
#   utw_enable(mytarget)
#
# PDCursesMod's VT port emits plain ANSI/VT escape sequences, which is exactly
# what xterm.js draws, and it is a complete curses implementation: wide
# characters, 256 colors, pads, panels, menus, mouse. Unlike wasm/curses/ it
# needs no termcap shim (it provides delay_output() and stubs tgetent() /
# tigetstr() itself), which is why UTW_TERMCAP defaults to OFF for this mode.
#
# The sources are the pinned wasm/PDCursesMod submodule. Compile-time
# adjustments for the browser:
#   PDC_WIDE, CHTYPE_32   wide-char API and 32-bit chtype, as Angband's
#                         attribute tables store colors in an int
#   usleep=...            PDC_napms() waits with usleep(); in the browser the
#                         wait must yield to the event loop instead of
#                         blocking it (see pdcurses-emscripten.c)

if(NOT DEFINED UTW_PDCURSES_DIR)
  set(UTW_PDCURSES_DIR "${UTW_ROOT}/PDCursesMod")
endif()

function(utw_pdcurses target)
  if(NOT EMSCRIPTEN)
    message(FATAL_ERROR
      "UTW_CURSES=pdcurses is for the Emscripten build; a native build "
      "should use the system curses")
  endif()
  if(NOT EXISTS "${UTW_PDCURSES_DIR}/curses.h")
    message(FATAL_ERROR
      "PDCursesMod sources missing at ${UTW_PDCURSES_DIR}; fetch the "
      "submodule with: git submodule update --init wasm/PDCursesMod")
  endif()

  if(NOT TARGET utw_pdcurses)
    add_library(utw_pdcurses STATIC
      # pdcurses/ (portable core, list mirrors common/libobjs.mif)
      "${UTW_PDCURSES_DIR}/pdcurses/addch.c"
      "${UTW_PDCURSES_DIR}/pdcurses/addchstr.c"
      "${UTW_PDCURSES_DIR}/pdcurses/addstr.c"
      "${UTW_PDCURSES_DIR}/pdcurses/attr.c"
      "${UTW_PDCURSES_DIR}/pdcurses/beep.c"
      "${UTW_PDCURSES_DIR}/pdcurses/bkgd.c"
      "${UTW_PDCURSES_DIR}/pdcurses/border.c"
      "${UTW_PDCURSES_DIR}/pdcurses/clear.c"
      "${UTW_PDCURSES_DIR}/pdcurses/color.c"
      "${UTW_PDCURSES_DIR}/pdcurses/debug.c"
      "${UTW_PDCURSES_DIR}/pdcurses/delch.c"
      "${UTW_PDCURSES_DIR}/pdcurses/deleteln.c"
      "${UTW_PDCURSES_DIR}/pdcurses/getch.c"
      "${UTW_PDCURSES_DIR}/pdcurses/getstr.c"
      "${UTW_PDCURSES_DIR}/pdcurses/getyx.c"
      "${UTW_PDCURSES_DIR}/pdcurses/inch.c"
      "${UTW_PDCURSES_DIR}/pdcurses/inchstr.c"
      "${UTW_PDCURSES_DIR}/pdcurses/initscr.c"
      "${UTW_PDCURSES_DIR}/pdcurses/inopts.c"
      "${UTW_PDCURSES_DIR}/pdcurses/insch.c"
      "${UTW_PDCURSES_DIR}/pdcurses/insstr.c"
      "${UTW_PDCURSES_DIR}/pdcurses/instr.c"
      "${UTW_PDCURSES_DIR}/pdcurses/kernel.c"
      "${UTW_PDCURSES_DIR}/pdcurses/keyname.c"
      "${UTW_PDCURSES_DIR}/pdcurses/mouse.c"
      "${UTW_PDCURSES_DIR}/pdcurses/move.c"
      "${UTW_PDCURSES_DIR}/pdcurses/outopts.c"
      "${UTW_PDCURSES_DIR}/pdcurses/overlay.c"
      "${UTW_PDCURSES_DIR}/pdcurses/pad.c"
      "${UTW_PDCURSES_DIR}/pdcurses/panel.c"
      "${UTW_PDCURSES_DIR}/pdcurses/printw.c"
      "${UTW_PDCURSES_DIR}/pdcurses/refresh.c"
      "${UTW_PDCURSES_DIR}/pdcurses/scanw.c"
      "${UTW_PDCURSES_DIR}/pdcurses/scr_dump.c"
      "${UTW_PDCURSES_DIR}/pdcurses/scroll.c"
      "${UTW_PDCURSES_DIR}/pdcurses/slk.c"
      "${UTW_PDCURSES_DIR}/pdcurses/termattr.c"
      "${UTW_PDCURSES_DIR}/pdcurses/terminfo.c"
      "${UTW_PDCURSES_DIR}/pdcurses/touch.c"
      "${UTW_PDCURSES_DIR}/pdcurses/util.c"
      "${UTW_PDCURSES_DIR}/pdcurses/window.c"
      # vt/ (the VT/ANSI port)
      "${UTW_PDCURSES_DIR}/vt/pdcclip.c"
      "${UTW_PDCURSES_DIR}/vt/pdcdisp.c"
      "${UTW_PDCURSES_DIR}/vt/pdcgetsc.c"
      "${UTW_PDCURSES_DIR}/vt/pdckbd.c"
      "${UTW_PDCURSES_DIR}/vt/pdcscrn.c"
      "${UTW_PDCURSES_DIR}/vt/pdcsetsc.c"
      "${UTW_PDCURSES_DIR}/vt/pdcutil.c")

    target_include_directories(utw_pdcurses PUBLIC
      "${UTW_PDCURSES_DIR}"
      "${UTW_PDCURSES_DIR}/vt")
    target_compile_options(utw_pdcurses PRIVATE -std=gnu99)
    # PUBLIC: consumers include <curses.h> and must see the same chtype and
    # wide-character API the library was built with.
    target_compile_definitions(utw_pdcurses PUBLIC
      PDC_WIDE
      CHTYPE_32)
    target_compile_definitions(utw_pdcurses PRIVATE
      _GNU_SOURCE
      usleep=utw_emscripten_usleep)
  endif()

  target_link_libraries(${target} PRIVATE utw_pdcurses)
  target_sources(${target} PRIVATE "${UTW_ROOT}/pdcurses-emscripten.c")
  # PDCurses provides delay_output(); keep tui.c's copy out of the link.
  target_compile_definitions(${target} PRIVATE UTW_NO_DELAY_OUTPUT)
  # PDCurses' <curses.h> must win over the empty stub in stub-include/.
  target_include_directories(${target} BEFORE PRIVATE "${UTW_PDCURSES_DIR}")
endfunction()
