# utw.cmake - CMake glue for the unix-tui-to-wasm runtime.
#
#   set(UTW_TITLE "my program")
#   include(<this dir>/wasm/utw.cmake)
#   utw_enable(mytarget)
#
# Options, all optional:
#   UTW_COLS, UTW_ROWS   terminal geometry (default 80x24)
#   UTW_PERSIST          persist the virtual FS in the browser (default ON)
#   UTW_PERSIST_DIR      IDBFS mount point (default /data)
#   UTW_TITLE            browser page title
#   UTW_PRELOAD_DIR      directory preloaded read-only at the FS root
#   UTW_WEB_DIR          servable bundle directory (default <build>/web)
#   UTW_WEB_FILES        extra files staged into the bundle
#   UTW_PTY_JS           xterm-pty glue (default <runtime>/emscripten-pty.js)
#   UTW_PAGE             page template (default <runtime>/index.html.in)
#   UTW_PRE_JS_EXTRA     extra --pre-js files, after the bundled pre.js
#
# utw_enable() compiles the termcap shim on every platform (so native builds
# keep working), adds the Emscripten link options, and creates the bundle
# target <target>_web, which stages the runtime plus the generated page.

if(NOT DEFINED UTW_ROOT)
  set(UTW_ROOT "${CMAKE_CURRENT_LIST_DIR}")
endif()
if(NOT DEFINED UTW_COLS)
  set(UTW_COLS 80)
endif()
if(NOT DEFINED UTW_ROWS)
  set(UTW_ROWS 24)
endif()
if(NOT DEFINED UTW_PERSIST)
  set(UTW_PERSIST ON)
endif()
if(NOT DEFINED UTW_PERSIST_DIR)
  set(UTW_PERSIST_DIR "/data")
endif()
if(NOT DEFINED UTW_TITLE)
  set(UTW_TITLE "unix-tui-to-wasm")
endif()
if(NOT DEFINED UTW_WEB_DIR)
  set(UTW_WEB_DIR "${CMAKE_BINARY_DIR}/web")
endif()
if(NOT DEFINED UTW_PTY_JS)
  set(UTW_PTY_JS "${UTW_ROOT}/emscripten-pty.js")
endif()
if(NOT DEFINED UTW_PAGE)
  set(UTW_PAGE "${UTW_ROOT}/index.html.in")
endif()

function(utw_enable target)
  target_sources(${target} PRIVATE "${UTW_ROOT}/tui.c")
  target_include_directories(${target} PRIVATE "${UTW_ROOT}")
  target_compile_definitions(${target} PRIVATE
    UTW_COLS=${UTW_COLS}
    UTW_ROWS=${UTW_ROWS})
  if(UTW_PERSIST)
    target_compile_definitions(${target} PRIVATE
      UTW_PERSIST_DIR="${UTW_PERSIST_DIR}")
  endif()

  if(NOT EMSCRIPTEN)
    return()
  endif()

  # Only the Emscripten build gets the <curses.h>/<termcap.h> stubs; a
  # native build must keep finding the real system headers.
  target_include_directories(${target} PRIVATE "${UTW_ROOT}/stub-include")

  if(NOT EXISTS "${UTW_PTY_JS}")
    message(FATAL_ERROR
      "xterm-pty glue missing (${UTW_PTY_JS}); run wasm/build-wasm.sh to fetch it")
  endif()

  target_link_options(${target} PRIVATE
    --minify 0
    -sMODULARIZE=1
    -sEXPORT_ES6=1
    -sASYNCIFY=1
    -sFORCE_FILESYSTEM=1
    -sALLOW_MEMORY_GROWTH=1
    -sEXIT_RUNTIME=1
    -sEXPORTED_RUNTIME_METHODS=FS
    -lidbfs.js
    --js-library=${UTW_PTY_JS}
    --pre-js=${UTW_ROOT}/pre.js)
  set(_link_deps "${UTW_ROOT}/pre.js;${UTW_PTY_JS}")
  if(DEFINED UTW_PRE_JS_EXTRA)
    foreach(_pre_js IN LISTS UTW_PRE_JS_EXTRA)
      target_link_options(${target} PRIVATE --pre-js=${_pre_js})
      list(APPEND _link_deps "${_pre_js}")
    endforeach()
  endif()
  if(DEFINED UTW_PRELOAD_DIR AND NOT UTW_PRELOAD_DIR STREQUAL "")
    target_link_options(${target} PRIVATE
      "--preload-file=${UTW_PRELOAD_DIR}@/")
  endif()

  set_target_properties(${target} PROPERTIES
    OUTPUT_NAME "${target}"
    SUFFIX ".js"
    LINK_DEPENDS "${_link_deps}")

  if(UTW_PERSIST)
    set(UTW_PERSIST_JS true)
  else()
    set(UTW_PERSIST_JS false)
  endif()
  set(UTW_MODULE "${target}.js")
  file(MAKE_DIRECTORY "${UTW_WEB_DIR}")
  configure_file("${UTW_PAGE}" "${UTW_WEB_DIR}/index.html" @ONLY)

  set(_out "$<TARGET_FILE_DIR:${target}>")
  add_custom_target(${target}_web ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${UTW_WEB_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${target}>" "${UTW_WEB_DIR}/${target}.js"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_out}/${target}.wasm" "${UTW_WEB_DIR}/${target}.wasm"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${UTW_ROOT}/storage.js" "${UTW_WEB_DIR}/storage.js"
    DEPENDS ${target}
    COMMENT "Staging web bundle in ${UTW_WEB_DIR}"
    VERBATIM)

  if(DEFINED UTW_PRELOAD_DIR AND NOT UTW_PRELOAD_DIR STREQUAL "")
    add_custom_command(TARGET ${target}_web POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${_out}/${target}.data" "${UTW_WEB_DIR}/${target}.data"
      VERBATIM)
  endif()
  if(DEFINED UTW_WEB_FILES)
    add_custom_command(TARGET ${target}_web POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${UTW_WEB_FILES} "${UTW_WEB_DIR}/"
      VERBATIM)
  endif()
endfunction()
