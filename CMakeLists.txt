cmake_minimum_required(VERSION 3.16)
project(unix-tui-to-wasm-demo C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(demo src/demo.c)
target_include_directories(demo PRIVATE src)

# --- browser terminal infrastructure -----------------------------------
# Set these before include()/utw_enable(); see README.md. CACHE variables
# keep the defaults overridable from the cmake command line.
set(UTW_TITLE "unix-tui-to-wasm demo" CACHE STRING "Browser page title")
set(UTW_COLS 80 CACHE STRING "Terminal columns")
set(UTW_ROWS 24 CACHE STRING "Terminal rows")
set(UTW_PERSIST ON CACHE BOOL "Persist the virtual filesystem in the browser")
set(UTW_PERSIST_DIR "/data" CACHE STRING "IDBFS mount point")

include(${CMAKE_CURRENT_SOURCE_DIR}/wasm/utw.cmake)
utw_enable(demo)
