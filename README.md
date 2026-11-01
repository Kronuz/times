# times

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/times/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/times/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)

Number-like timespec for C++20: wall-clock reads, seconds as a double, arithmetic and
comparisons with nanosecond carry. `timespec_t` derives from `struct timespec`, so it
passes straight to system calls, and every operation folds the nanoseconds back into
`[0, 1e9)`. Reach for it in code that speaks `timespec` to the operating system and wants
arithmetic on it.

## ✨ Highlights

- **Still a `timespec`:** `timespec_t` adds methods and operators, no fields, so a
  pointer to one is a `struct timespec*`.
- **Seconds in and out:** `timespec_t(1.5)` and `as_double()`.
- **Arithmetic:** `+` and `-` between two values, `*` and `/` by a `double` factor,
  each normalized so a carry or borrow lands in `tv_sec`.
- **Comparisons:** `<`, `<=`, `>`, `>=`, `==` and `!=`, including a `double` on the
  left (`0.5 < ts`).
- **`now()` and `set_now()`** read `CLOCK_REALTIME`, with a Mach fallback on macOS
  systems that lack `clock_gettime`.

## 🚀 Quick start

```cpp
#include "times.h"

#include <cstdio>

int main() {
	timespec_t deadline = now() + timespec_t(1.5);  // 1.5 s from now
	timespec_t total = timespec_t(0.6) + timespec_t(0.7);
	std::printf("%.1f s\n", total.as_double());        // 1.3 s, nanoseconds carried
	std::printf("%.2f s\n", (total * 0.5).as_double()); // 0.65 s
	std::printf("%d\n", 0.5 < deadline);               // 1
}
```

## 📦 Installation

Requires C++20 and CMake 3.14 or newer; no other dependencies. CI builds and tests it on
Linux (x86_64 with GCC and Clang, arm64), macOS (arm64, x86_64) and FreeBSD, plus ASan,
UBSan and TSan builds.

```cmake
include(FetchContent)
FetchContent_Declare(times
  GIT_REPOSITORY https://github.com/Kronuz/times.git
  GIT_TAG        666dfdd85edf74a5c61c7467b4deb298697ace0c)  # or main, to track the tip
FetchContent_MakeAvailable(times)

target_link_libraries(my_app PRIVATE times::times)
```

`times::times` is a small compiled library (`times.cc`, the macOS fallback) with a
CMake-generated `times_config.h`. Include `times.h`.

## 🎯 When to use it

- Absolute times and timeouts for APIs that take a `struct timespec`.

Not for:

- Measuring elapsed time: `now()` reads the wall clock, which jumps when it is set or
  stepped; use `std::chrono::steady_clock`.
- Code with no `timespec` in sight: `std::chrono` is the standard choice.
- Seconds beyond the range of `int`: `timespec_t(double)` converts the whole seconds
  through `int`, so a value past about 2.1e9 s (a Unix time after January 2038) does not
  fit. `*` and `/` go through the same conversion.
- A `double` compared with a temporary: the `double`-first comparisons take a
  `timespec_t&`, so `0.5 < now()` does not compile; store the value first.

## 📚 Documentation

- [docs/API.md](docs/API.md): `timespec_t`, `now()`, the operators and which clock it
  reads.
- [ARCHITECTURE.md](ARCHITECTURE.md): normalization and the platform fallback.
- [AGENTS.md](AGENTS.md): repository map, invariants and traps.
- [Catalog entry](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#times) in
  Kronuz C++ Libraries.

## 🛠️ Building and testing

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## 🤝 Contributing

Issues and pull requests are welcome; read [AGENTS.md](AGENTS.md) and the catalog's
[rules of engagement](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#rules-of-engagement)
first.

## 📄 License

MIT, see [LICENSE](LICENSE). Extracted from [Xapiand](https://github.com/Kronuz/Xapiand).
