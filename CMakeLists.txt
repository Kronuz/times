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
  # One executable per claim, so a failure names it: seconds beyond int and negative values,
  # a double compared on the left with a temporary, and the old constructor as a reference.
  foreach(name large_seconds rvalue_compare diff)
    add_executable(times_${name} test/${name}.cc)
    target_link_libraries(times_${name} PRIVATE times)
    add_test(NAME times_${name} COMMAND times_${name})
  endforeach()
  # The constructor has three shapes (TIMES_CTOR_SHAPE) with the same value; the two tests that
  # check values run each shape on every platform.
  foreach(shape 0 1 2)
    foreach(name large_seconds diff)
      add_executable(times_${name}_shape${shape} test/${name}.cc)
      target_link_libraries(times_${name}_shape${shape} PRIVATE times)
      target_compile_definitions(times_${name}_shape${shape} PRIVATE TIMES_CTOR_SHAPE=${shape})
      add_test(NAME times_${name}_shape${shape} COMMAND times_${name}_shape${shape})
    endforeach()
  endforeach()
  # The benchmark (bench/bench.cc), run by hand: the library, and the header of d762ec6 (bench/old/times.h)
  # twice (the second copy gives the A/A spread). `python3 bench/abcmp.py build` pairs them. The CTest
  # tests run each once; they time nothing.
  option(TIMES_BUILD_BENCH "Build the benchmark in bench/" OFF)
  if(TIMES_BUILD_BENCH)
    add_executable(times_bench_new bench/bench.cc)
    target_link_libraries(times_bench_new PRIVATE times)
    add_test(NAME times_bench_new COMMAND times_bench_new)
    foreach(copy old old2)
      add_executable(times_bench_${copy} bench/bench.cc)
      target_compile_features(times_bench_${copy} PRIVATE cxx_std_20)
      # Only the old header: no times::times, so the current times.h is not on the include path.
      target_include_directories(times_bench_${copy} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/bench/old" "${CMAKE_CURRENT_BINARY_DIR}")
    endforeach()
    add_test(NAME times_bench_old COMMAND times_bench_old)
  endif()
endif()
