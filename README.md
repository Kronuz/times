# times

[![CI](https://github.com/Kronuz/times/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/times/actions/workflows/ci.yml)

A small **monotonic / wall-clock time** helper for C++20, extracted from
[Xapiand](https://github.com/Kronuz/Xapiand).

## What it is

One header, `times.h`, plus a tiny `times.cc` for the macOS fallback. It gives
you `timespec_t`, a thin `struct timespec` subclass that behaves like a number:
read the clock into it, convert it to and from a `double` count of seconds, add
and subtract intervals (with nanosecond carry handled for you), and compare two
of them. A free `now()` returns the current realtime clock as a `timespec_t`.

The surface:

- **`timespec_t`** derives from `timespec`, so it still has the raw `tv_sec` /
  `tv_nsec` fields. Construct it empty (zeroed), or from a `double` number of
  seconds (`timespec_t(3.25)` is 3 s + 250 ms).
- **`now()`** returns a `timespec_t` set to `CLOCK_REALTIME`.
- **`set_now()`** refreshes an existing `timespec_t` from the clock.
- **`as_double()`** collapses it back to a `double` count of seconds.
- **`clear()`** zeroes it.
- **Arithmetic:** `+` and `-` between two `timespec_t`, and `*` / `/` to scale a
  duration by a scalar factor (a `double`). `_adjust()` normalizes the nanosecond
  field back into the `[0, 1e9)` range after every operation so a carry or borrow
  lands in `tv_sec`.
- **Comparisons:** the full `< <= > >= == !=` set, both between two `timespec_t`
  and with a plain `double` on the left-hand side (`0.5 < ts`).

## Install

CMake with `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(
  times
  GIT_REPOSITORY https://github.com/Kronuz/times.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(times)

target_link_libraries(your_target PRIVATE times::times)
```

The `times` target is a small `STATIC` library: it compiles `times.cc` (which is
empty on any platform that has `clock_gettime`, including modern macOS), requests
`cxx_std_20`, and puts both the source dir and the generated config header on your
include path. Then:

```cpp
#include "times.h"
```

Requires C++20. On macOS it builds with AppleClang/libc++ or Homebrew LLVM, the
same toolchains Xapiand uses. The header keeps its original filename, so a codebase
that already `#include "times.h"` just needs this repo on its include path.

## Usage

```cpp
#include "times.h"
#include <cstdio>

// Time a piece of work.
timespec_t start = now();
do_some_work();
double secs = (now() - start).as_double();
std::printf("took %.3f s\n", secs);

// Convert to and from a plain double count of seconds.
timespec_t deadline(1.5);          // 1 s + 500 ms
double d = deadline.as_double();   // 1.5

// Arithmetic carries the nanosecond field automatically.
timespec_t total = timespec_t(0.6) + timespec_t(0.7);   // 1.3 s, carried
bool soon = 0.5 < deadline;        // double on the left-hand side works too
```

## Build & test

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

The test takes two clock readings around a short sleep and checks the clock does
not run backwards and reads a plausible absolute time, measures an elapsed
interval by subtraction (loose bounds, so no flakiness), round-trips a `double`
through `timespec_t`, exercises the `+`/`-` nanosecond carry and borrow across the
1e9 boundary, runs the comparison operators (member and double-lhs forms), and
checks `clear()`. It prints `all times tests passed` and exits 0.

## Provenance

Extracted from [Xapiand](https://github.com/Kronuz/Xapiand). The one coupling was
`#include "config.h"`, used solely for the `HAVE_CLOCK_GETTIME` macro. Standalone,
the library detects `clock_gettime` itself: CMake's `check_symbol_exists` writes
the result into a generated `times_config.h` that `times.h` includes instead. The
`__linux__` / `__APPLE__` branches and the Mach fallback are preserved verbatim.
The Mach fallback in `times.cc` carries a second, BSD-style copyright (MM Weiss).
See [ARCHITECTURE.md](ARCHITECTURE.md) for the design and [AGENTS.md](AGENTS.md)
for the repo map and invariants.

## License

MIT, Copyright (c) 2015-2018 Dubalu LLC. See [LICENSE](LICENSE). The macOS Mach
`clock_gettime` fallback in `times.cc` additionally carries a BSD-style copyright
(MM Weiss), retained in that file's header.
