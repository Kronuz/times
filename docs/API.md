# times API reference

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

## Usage

```cpp
#include "times.h"
#include <cstdio>

// Convert to and from a plain double count of seconds.
timespec_t deadline(1.5);          // 1 s + 500 ms
double d = deadline.as_double();   // 1.5

// Arithmetic carries the nanosecond field automatically.
timespec_t total = timespec_t(0.6) + timespec_t(0.7);   // 1.3 s, carried
bool soon = 0.5 < deadline;        // double on the left-hand side works too
```

## Which clock

`now()` and `set_now()` read `CLOCK_REALTIME`, the wall clock, which can jump
when NTP steps it or someone sets it. Use it for timestamps. To measure elapsed
time, use `std::chrono::steady_clock`.
