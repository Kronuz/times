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
CMakeLists.txt               STATIC library `times` (+ alias times::times); check_symbol_exists + configure_file feature detection; CTest test `times`.
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
`times_test`; the registered CTest name is `times`.

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
- **Every arithmetic op normalizes via `_adjust()`.** After touching `tv_nsec`,
  the nanosecond field must be walked back into `[0, 1e9)` with the overflow
  carried into `tv_sec`. If you add an operator, call `_adjust()` the same way, or
  you will produce out-of-range `timespec` values.
- **Comparisons go through `_timespec_cmp`:** seconds first, nanoseconds only on a
  tie. Keep both the member forms and the free `double`-lhs forms in sync; they
  must agree.
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
- **The `double` constructor is `explicit`.** `timespec_t(3.25)` works, but a bare
  `3.25` won't implicitly convert in a `timespec_t` context. The free comparison
  operators exist precisely so `double`-vs-`timespec_t` comparisons still read
  naturally; don't make the constructor implicit to "simplify" that.
- **`*` and `/` scale by a scalar (`double`), not by another `timespec_t`.** They
  operate on the whole value via `as_double()` (a meaningful multiple/fraction of
  the duration), then `_adjust()` re-normalizes `tv_nsec`. This diverges from
  upstream Xapiand, whose `*`/`/` multiplied the sec/nsec fields componentwise and
  whose `operator/` was a verbatim copy of `operator*` (a bug: it multiplied). The
  fix lives here; mirror it upstream when reconciling.

## Standalone vs. Xapiand

This is a standalone extraction from
[Xapiand](https://github.com/Kronuz/Xapiand). The time logic in `times.h` and the
Mach fallback in `times.cc` are close to the original, with three deltas: the
`config.h` -> generated `times_config.h` include swap described above, the one-line
anchor symbol in `times.cc` that keeps the static archive linkable on macOS, and the
`*`/`/` operators, which now scale by a scalar (the upstream versions multiplied the
fields componentwise, and `operator/` was a copy of `operator*`). Keep the rest that
way so changes here stay reconcilable
with upstream as a plain edit. The Mach fallback in `times.cc` carries a separate
BSD-style copyright (MM Weiss), retained in that file.
