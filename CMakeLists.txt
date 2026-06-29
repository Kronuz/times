cmake_minimum_required(VERSION 3.14)
project(times LANGUAGES CXX)

include(CheckSymbolExists)

# --- Feature detection: clock_gettime ---
# times.h used Xapiand's project-wide "config.h" for one thing only: the
# HAVE_CLOCK_GETTIME macro. Standalone, the library detects that itself. We probe
# for clock_gettime and configure_file() the result into a tiny generated header
# (times_config.h) that times.h includes in place of "config.h". On macOS the
# symbol is present, so HAVE_CLOCK_GETTIME is defined and the Mach fallback in
# times.cc compiles to nothing; on a platform lacking it, the fallback kicks in.
check_symbol_exists(clock_gettime "time.h" HAVE_CLOCK_GETTIME)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/times_config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/times_config.h"
  @ONLY
)

# --- Library target ---
# times is not header-only: times.cc carries the macOS Mach fallback definition
# of clock_gettime (compiled in only when HAVE_CLOCK_GETTIME is absent), so it
# must be compiled and linked. The repo root holds times.h; the build dir holds
# the generated times_config.h, so both go on the include path.
add_library(times times.cc)
target_include_directories(times PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_features(times PUBLIC cxx_std_20)

# Alias so consumers can link `times::times` whether they add this via
# add_subdirectory or FetchContent.
add_library(times::times ALIAS times)

# Only build the test when this repo is the top-level project, so consumers
# (e.g. via FetchContent) don't build our tests.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
  add_executable(times_test test/test.cc)
  target_link_libraries(times_test PRIVATE times)
  add_test(NAME times COMMAND times_test)
endif()
