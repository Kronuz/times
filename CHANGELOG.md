# Changelog

All notable changes to `times`. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); the library has no
releases, so entries are dated.

## 2026-11-01

### Fixed

- `timespec_t(double)` keeps whole seconds past `int`. It converted them through `int`, and
  the conversion is undefined beyond the range of `int`: `timespec_t(3e9)` was
  `tv_sec = 2147483647`, `tv_nsec = 2147483647` on arm64 (the conversion saturates) and
  `tv_sec = -2147483648`, `tv_nsec = -2147483648` on x86 (it returns `INT_MIN`). A Unix
  time after January 2038 did not fit. The whole seconds now go through `time_t`, 64 bits
  on every supported platform, so `timespec_t(3e9)` is `tv_sec = 3000000000`, `tv_nsec = 0`.
  `*` and `/`, which build their result through that constructor, keep large values too:
  `timespec_t(3e9) * 2.0` is 6e9 s.
- A negative non-integral `double` builds a normalized value. The constructor stored the
  fraction of `-0.5` as `tv_sec = 0`, `tv_nsec = -500000000`, so it compared unequal to
  the same instant made by arithmetic (`timespec_t(0.5) - timespec_t(1.0)` is
  `-1`, `500000000`). It now borrows one second like every other operation, so `tv_nsec`
  is in `[0, 1e9)` and `timespec_t(-0.5)` is `tv_sec = -1`, `tv_nsec = 500000000`. The
  value is unchanged (the constructor still truncates the fraction toward zero at the
  nanosecond, as `std::chrono::duration_cast` does); only the fields are folded.
- The comparisons with a `double` on the left take the `timespec_t` by `const` reference.
  They took `timespec_t&`, so `0.5 < now()` and a comparison with a `const timespec_t`
  did not compile. All six (`< <= > >= == !=`) are fixed.

### Added

- `test/large_seconds.cc` (`times_large_seconds`): a table of whole seconds from -2^62 to
  2^62 written as decimal integers; 200,000 values `a + b/4096` with `|a| < 2^40`, exact in
  a double, whose seconds and nanoseconds are worked out in 64-bit integer arithmetic (and,
  for `|a| < 2^20`, with `std::chrono::duration_cast`); every multiple of 1/8 from -20 to
  20 against 1/8-second steps by `+=` and `-=`, in order and by equality; 100,000
  random pairs with seconds up to 4e9 for `+`, `-` and the six comparisons against
  `std::chrono::nanoseconds` arithmetic; `*` and `/` beyond `int`; and a deadline
  `now() + 1e9 s` through a double.
- `test/rvalue_compare.cc` (`times_rvalue_compare`): `0.5 < now()` and the other five
  operators with a temporary, a `const` object and a result of arithmetic, and a grid of
  289 pairs that agree with the member operators.
- `test/diff.cc` (`times_diff`): the constructor, `*` and `/` against the frozen old
  code in `test/old/`. For `0 <= x < 2^31` and for negative integers the fields are
  identical (about 600,000 inputs); `*` and `/` give identical fields for results in
  range. One class differs: a negative value with a fraction of a nanosecond or more,
  where the old `tv_nsec` was negative and the new one is folded (the value, within 1e-6 s,
  is the same, and the old value folded by an addition of zero is the new value). Values
  with `|x| >= 2^31` are not run against the old code, which is undefined there.
- The tests that use `assert` keep it live under `NDEBUG`, so the README's Release build
  runs them; the new tests use their own `CHECK`.
