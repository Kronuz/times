# Architecture

The internal design of `times`: a `timespec` subclass that reads the clock and
behaves like a seconds-valued number, plus a CMake-driven feature probe that
replaces Xapiand's project-wide `config.h`. For usage see `README.md`; for the
repo map and invariants see `AGENTS.md`.

## Shape

A header, a small source file, and a generated config header:

```
  times.h            timespec_t, now(), the operators; includes times_config.h
  times.cc           macOS Mach fallback for clock_gettime (empty when present)
  times_config.h.in  template; CMake configure_file's it to times_config.h
```

## timespec_t

The core type derives from `struct timespec` rather than wrapping it, so it keeps
the raw `tv_sec` / `tv_nsec` fields and can be passed straight to `clock_gettime`.
On top of that it adds value semantics:

- A default constructor that zeroes the fields, and an explicit `double`
  constructor that splits a seconds count into whole seconds (`tv_sec`) and the
  fractional remainder scaled to nanoseconds (`tv_nsec`).
- `set_now()` calls `clock_gettime(CLOCK_REALTIME, this)` in place; the free
  `now()` is just a `timespec_t` that has had `set_now()` called on it.
- `as_double()` reassembles the two fields into a single `double`.

## The nanosecond carry

Arithmetic is where the type earns its keep. `+=` and `-=` operate on the two
fields directly and then call `_adjust()`, which walks `tv_nsec` back into the
`[0, 1e9)` range: while it is negative it adds a second's worth of nanoseconds and
decrements `tv_sec` (a borrow), and while it is at or past `1e9` it subtracts and
increments `tv_sec` (a carry). Without this, adding `0.6 s` and `0.7 s` would leave
`tv_nsec` at `1.3e9`, an out-of-range `timespec`. `*=` and `/=` take a plain
`double` and scale the whole duration (via `as_double()`), then normalize the same
way. The binary operators (`+`, `-`, `*`, `/`) are written in terms of the in-place
forms, so they get the same normalization. The test pins this by adding two
sub-second values whose nanosecond parts overflow and checking the carry lands in
`tv_sec`.

## Comparisons

The comparison operators go through one macro, `_timespec_cmp`, which compares
`tv_sec` first and falls back to `tv_nsec` only on a tie. The full set
(`< <= > >= == !=`) exists both as members (two `timespec_t`) and as free
functions taking a `double` on the left, so `0.5 < ts` reads naturally; those free
forms just wrap the left operand in a `timespec_t` and defer to the member
comparison.

## Feature detection: replacing config.h

Upstream, `times.h` opened with `#include "config.h"` purely to learn whether the
platform has `clock_gettime`, via `HAVE_CLOCK_GETTIME`. That macro gates the macOS
branch: when `clock_gettime` is absent, the header declares a Mach-based
replacement and `times.cc` defines it; when it is present, both the declaration
and definition vanish and the system call is used directly.

Standalone, there is no project `config.h`, so the library detects the symbol
itself. `CMakeLists.txt` runs `check_symbol_exists(clock_gettime "time.h"
HAVE_CLOCK_GETTIME)` and `configure_file`s `times_config.h.in` into the build
directory as `times_config.h`, with `#cmakedefine HAVE_CLOCK_GETTIME 1` resolving
to a real `#define` or nothing. `times.h` includes that generated header in place
of `config.h`. The `__linux__` / `__APPLE__` structure and the Mach fallback are
otherwise untouched, so on modern macOS (where `clock_gettime` exists) the probe
defines the macro and `times.cc` compiles to an empty translation unit, exactly as
it did inside Xapiand.

## Why STATIC, not header-only

Unlike the other header-only siblings, `times` ships a `.cc`. It is empty wherever
`clock_gettime` exists, but on a platform that lacks it (older macOS) `times.cc`
holds the only definition of the Mach `clock_gettime`. That definition has to be
compiled and linked somewhere, so the CMake target is a `STATIC` library rather
than a pure `INTERFACE` one. The cost is one (often empty) object file.
