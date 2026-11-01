# AGENTS.md

Working notes for agents modifying this repository. For the design read
`ARCHITECTURE.md`; for usage read `README.md`. This file covers the repo layout,
how to build and test, the invariants you must not break, and the traps that are
easy to fall into.

## Repo map

```
times.h                      timespec_t (timespec subclass), now(), set_now/as_double/clear, +/-/*// and comparison operators. Header. From Xapiand, config.h decoupled.
times.cc                     macOS Mach fallback definition of clock_gettime; compiles to nothing when HAVE_CLOCK_GETTIME is set. From Xapiand (BSD-licensed fragment, MM Weiss).
times_config.h.in            Template for the generated feature-detection header. CMake configure_file's it.
test/test.cc                 Runnable smoke test: clock advance, elapsed via subtraction, double round-trip, nanosecond carry, comparisons, clear().
test/large_seconds.cc        Seconds past int, negative values, +/-/comparisons and * / beyond int, against decimal tables, exact integer arithmetic and std::chrono.
test/rvalue_compare.cc       A double on the left of a temporary and of a const timespec_t (`0.5 < now()`).
test/diff.cc                 The constructor, * and / against the frozen old code (test/old/); the one result class that differs is listed and asserted.
test/check.h                 CHECK(), which stays live under NDEBUG.
bench/bench.cc               The timespec_t rows, built against the library and against bench/old/times.h, the header of d762ec6 byte for byte (TIMES_BUILD_BENCH); bench/abcmp.py pairs the runs.
CMakeLists.txt               STATIC library `times` (+ alias times::times); check_symbol_exists + configure_file feature detection; CTest tests `times`, `times_large_seconds`, `times_rvalue_compare`, `times_diff`, and `times_large_seconds` and `times_diff` again with each constructor shape forced (`_shape0` to `_shape2`), and with the bench option `times_bench_new`, `times_bench_old`.
LICENSE                      MIT, Copyright (c) 2015-2018 Dubalu LLC.
README.md                    What it is, install, quick start.
docs/API.md                  What it provides and usage.
ARCHITECTURE.md              Internal design: timespec_t, the nanosecond carry, feature detection.
```

The library is a thin `STATIC` target: it compiles `times.cc` (empty wherever
`clock_gettime` exists) and puts the source dir and the generated config header on
the include path. It is not header-only because `times.cc` carries the only
definition of the macOS Mach fallback.

## Build and run the test

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

On macOS prefer Homebrew LLVM for sanitizer-clean builds, matching the family:

```sh
cmake -B build -DCMAKE_CXX_COMPILER=$(brew --prefix llvm)/bin/clang++ \
  && cmake --build build && ctest --test-dir build
```

Expected output ends with `all times tests passed`, exit 0. The test target is
`times_test`; the registered CTest names are `times`, `times_large_seconds`,
`times_rvalue_compare`, `times_diff`, and the `_shape0` to `_shape2` forms of the second and
fourth. `test/test.cc` undefines `NDEBUG` so its
`assert`s run in a Release build; the other tests use `CHECK`.

## Verification and benchmarks

The method shared by the Kronuz libraries is in
[docs/PERFORMANCE.md](https://github.com/Kronuz/cpp-libraries/blob/main/docs/PERFORMANCE.md).

- **Tests.** `cmake -B build && cmake --build build && ctest --test-dir build` runs ten CTest
  tests. The `_shape0` to `_shape2` copies of `times_large_seconds` and `times_diff` force each
  `TIMES_CTOR_SHAPE`, so every constructor shape is checked on every platform.
- **Differential.** `times_diff` (`test/diff.cc`) compares the constructor, `*` and `/` with the
  frozen code of `d762ec6` in `test/old/times_old.h` (never edited). The result classes and what
  the test asserts on each are listed at its top: identical fields where the old code was defined
  and right, the same value with folded fields for a negative fraction, and on x86 the decimal
  where the old `long double` product was one nanosecond short.
- **Oracles.** `times_large_seconds` checks against decimal tables, exact 64-bit integer
  arithmetic and `std::chrono` (`duration_cast`, `nanoseconds` arithmetic);
  `times_rvalue_compare` checks the free comparisons against the member operators.
- **Sanitizers.** CI (`.github/workflows/ci.yml`, which calls the shared
  [cmake.yml](https://github.com/Kronuz/cpp-libraries/blob/main/.github/workflows/cmake.yml))
  runs every test in an ASan+UBSan build and a TSan build, with clang on Linux. The ASan+UBSan
  job is:

  ```sh
  CC=clang CXX=clang++ cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -D_GLIBCXX_ASSERTIONS" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
  cmake --build build-asan && ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-asan --output-on-failure
  ```

- **Benchmark.** The rows of the CHANGELOG's Performance table are `bench/bench.cc`, built three
  times: against the library (`times_bench_new`) and twice against `bench/old/times.h`, the header
  of `d762ec6` byte for byte (`times_bench_old`, `times_bench_old2`, whose ratio to the first is
  the A/A spread). `test/old/times_old.h` has no `now()` or comparisons, so it cannot serve two of the rows. CI
  builds the three and runs each once, which times nothing. Time on an otherwise idle machine,
  under a hold (the shared method above), on arm64 and x86:

  ```sh
  cmake -B build -DCMAKE_BUILD_TYPE=Release -DTIMES_BUILD_BENCH=ON && cmake --build build
  python3 bench/abcmp.py build 5     # best of 5 rounds, binaries back to back
  ```

- **Not here.** The repository has no fuzz target or mutation script.

## Conventions

- **C++20.** The target requests `cxx_std_20` to stay uniform with the sibling
  libraries; the header itself is older-standard-friendly, but don't drop the
  target below it.
- **One real dependency, on the platform clock.** The only includes are system
  time headers (`<sys/time.h>`, `<time.h>`, the Mach headers in the fallback) plus
  the generated `times_config.h`. Don't add anything else, and don't reintroduce a
  project-wide `config.h`.
- **Filenames are stable.** The header keeps its original Xapiand name
  (`times.h`); `times.cc` keeps its name too. A consumer that already
  `#include`s `times.h` just needs this repo on the include path. Don't rename them.
- Tabs for indentation, double quotes in code, no em dashes in prose.

## Load-bearing invariants

- **`timespec_t` derives from `timespec`.** It is not a wrapper. The raw
  `tv_sec` / `tv_nsec` fields stay public (inherited) so the object can be passed
  straight to `clock_gettime`. Don't turn it into a class with private fields; the
  `set_now()` call passes `this` to a C API expecting a `struct timespec *`.
- **Every arithmetic op normalizes via `_adjust()`, and the `double` constructor folds
  its fraction the same way.** After touching `tv_nsec`, the nanosecond field must be
  walked back into `[0, 1e9)` with the overflow carried into `tv_sec`. If you add an
  operator, call `_adjust()` the same way, or you will produce out-of-range `timespec`
  values. A negative `double` truncates to a negative `tv_nsec`, which the constructor
  borrows one second for.
- **Seconds are 64 bits, never an `int`.** The constructor converts the whole seconds with
  a 64-bit conversion; an `int` there wraps or saturates past 2038.
- **The constructor has three shapes with the same value** (`TIMES_CTOR_SHAPE`): a 64-bit
  conversion and its inverse (any target), a `trunc` instruction with 64-bit lanes (arm64),
  and a `trunc` with a 32-bit nanosecond count (x86 with SSE4.1). They exist because the
  compiler vectorizes a loop of constructions only with the shape that fits the target;
  measure the array and the dependent-chain rows of the 2026-11-01 Performance table in
  `CHANGELOG.md` on arm64 and x86-64 before changing one. Each value test runs every shape
  on every platform
  (`times_large_seconds_shape0` to `_shape2`, `times_diff_shape0` to `_shape2`).
- **The borrow is arithmetic, not a branch.** `ns >> 63` (or `>> 31`) selects it, so a loop
  of constructions vectorizes; a `[[unlikely]]` branch there kept it scalar and 40% slower.
  Rounding stays toward zero at the nanosecond, so a negative decimal is the mirror of the
  positive one; a `floor` would be one nanosecond below the decimal for a fifth of them.
- **The fraction is scaled in `double`, never `long double`.** A `long double` product on x86
  came out one nanosecond short for about half of the decimals (0.3 s was 299999999 ns) and
  differed from arm64, whose `long double` is a `double`.
- **Comparisons go through `_timespec_cmp`:** seconds first, nanoseconds only on a
  tie. Keep both the member forms and the free `double`-lhs forms in sync; they
  must agree. The free forms take `const timespec_t&`, so a temporary binds.
- **`HAVE_CLOCK_GETTIME` comes from the generated header, not a hand-edit.** It is
  produced by CMake's `check_symbol_exists` into `times_config.h`. Don't hard-code
  it in `times.h` or `times.cc`.

## The config.h decoupling

This is the one change from upstream. Xapiand's `times.h` did
`#include "config.h"` solely for `HAVE_CLOCK_GETTIME`. Standalone:

- `CMakeLists.txt` runs `check_symbol_exists(clock_gettime "time.h"
  HAVE_CLOCK_GETTIME)` and `configure_file`s `times_config.h.in` into the build
  dir as `times_config.h` (`#cmakedefine HAVE_CLOCK_GETTIME 1`).
- `times.h` includes `"times_config.h"` in place of `"config.h"`. Everything else
  (the `__linux__` / `__APPLE__` branches, the Mach fallback) is verbatim.
- The build directory is on the include path so the generated header resolves.

If you build `times.h` outside CMake, you must provide a `times_config.h` on the
include path yourself (an empty one is fine where `clock_gettime` exists), or the
include will fail. That is intentional: the header's feature detection is external,
exactly as it was when `config.h` came from the parent project.

## Traps

- **`times.cc` is nearly empty on macOS, and that's correct.** With
  `HAVE_CLOCK_GETTIME` defined, the Mach fallback is entirely `#ifdef`'d out. The
  one thing that survives is a single anchor symbol
  (`times_translation_unit_anchor`): an object with no symbols at all makes
  `libtimes.a` a member that Apple's `ld` rejects ("not a mach-o file"), so the
  anchor keeps the translation unit non-empty and the archive linkable. Don't
  remove it, and don't "fix" the empty look by deleting the file; on a platform
  without `clock_gettime` it provides the only definition of the fallback.
- **A `double` carries 53 bits.** `timespec_t(double)`, `as_double()`, `*` and `/` round
  to a double's precision: about 0.24 µs at the current Unix time, 1 ns or better below
  97 days. Do not use them to carry exact nanoseconds; `+`, `-` and the comparisons work
  on the integer fields and are exact. The constructor is undefined for a NaN, an
  infinity or a value outside the range of `time_t`, as `std::chrono::duration_cast` is.
- **The `double` constructor is `explicit`.** `timespec_t(3.25)` works, but a bare
  `3.25` won't implicitly convert in a `timespec_t` context. The free comparison
  operators exist precisely so `double`-vs-`timespec_t` comparisons still read
  naturally; don't make the constructor implicit to "simplify" that.
- **`*` and `/` scale by a scalar (`double`), not by another `timespec_t`.** They
  operate on the whole value via `as_double()` (a meaningful multiple/fraction of
  the duration) and build the result through the constructor, which leaves `tv_nsec` in
  `[0, 1e9)`. This diverges from
  upstream Xapiand, whose `*`/`/` multiplied the sec/nsec fields componentwise and
  whose `operator/` was a verbatim copy of `operator*` (a bug: it multiplied). The
  fix lives here; mirror it upstream when reconciling.

## Standalone vs. Xapiand

This is a standalone extraction from
[Xapiand](https://github.com/Kronuz/Xapiand). The time logic in `times.h` and the
Mach fallback in `times.cc` are close to the original, with four deltas: the
`config.h` -> generated `times_config.h` include swap described above, the one-line
anchor symbol in `times.cc` that keeps the static archive linkable on macOS, the
`*`/`/` operators, which scale by a scalar (the upstream versions multiplied the
fields componentwise, and `operator/` was a copy of `operator*`), and the `double`
constructor and the `double`-lhs comparisons, which use 64-bit seconds, scale the fraction in
`double`, normalize a negative value and take `const timespec_t&` (see `CHANGELOG.md`). Keep the rest that
way so changes here stay reconcilable
with upstream as a plain edit. The Mach fallback in `times.cc` carries a separate
BSD-style copyright (MM Weiss), retained in that file.
