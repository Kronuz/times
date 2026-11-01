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
- The fraction is scaled in `double` on every platform. The constructor scaled it in
  `long double`, which on x86 (80-bit) came out one nanosecond short for about half of the
  decimal fractions: `timespec_t(0.3)` had `tv_nsec = 299999999` (0.6 and 0.7 likewise), so
  `timespec_t(0.3) + timespec_t(0.7)` was 999999998 ns and not one second. arm64, whose
  `long double` is a `double`, gave 300000000. A decimal with one to three digits now gives
  the nanoseconds it says on both (all 1107 of them; with four or more digits about 2% come
  out one nanosecond short, as they did on arm64). Of 1,111,104 decimals of one to six digits,
  536,298 changed on x86 and none on arm64. The test asserts the decimals and, on a platform
  with a wide `long double`, that the old result is one nanosecond short exactly where it
  differs.
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
  `std::chrono::nanoseconds` arithmetic; `*` and `/` beyond `int`; a deadline
  `now() + 1e9 s` through a double; and every decimal of one to three digits (1107 values,
  both signs) against the nanoseconds it says, plus `0.3 + 0.7 == 1.0`, which fail on x86
  against the old header (299999999 ns for 0.3).
- `test/rvalue_compare.cc` (`times_rvalue_compare`): `0.5 < now()` and the other five
  operators with a temporary, a `const` object and a result of arithmetic, and a grid of
  289 pairs that agree with the member operators.
- `test/diff.cc` (`times_diff`): the constructor, `*` and `/` against the frozen old
  code in `test/old/`. For `0 <= x < 2^31` and for negative integers the fields are
  identical (about 600,000 inputs); `*` and `/` give identical fields for results in
  range. Two classes differ: a negative value with a fraction of a nanosecond or more,
  where the old `tv_nsec` was negative and the new one is folded (the value, within 1e-6 s,
  is the same, and the old value folded by an addition of zero is the new value); and, on a
  platform whose `long double` is wider than `double` (x86), a decimal that the old
  `long double` product left one nanosecond short, where the test asserts old == new - 1
  and new == the decimal (the other classes allow that nanosecond there). Values with
  `|x| >= 2^31` are not run against the old code, which is undefined there.
- The tests that use `assert` keep it live under `NDEBUG`, so the README's Release build
  runs them; the new tests use their own `CHECK`.

### Changed

- `_adjust()` compares and adds with the integer constant `1000000000L`; it used `1e9L`, a
  `long double`, so every `+=` and `-=` converted `tv_nsec` through the floating-point unit.
  Results are identical.
- `*=` and `/=` no longer call `_adjust()` after building the result: the constructor
  already leaves `tv_nsec` in `[0, 1e9)`.
- The constructor has three shapes with the same value, chosen by target
  (`TIMES_CTOR_SHAPE`, see Performance). The tests that check values run each shape on every
  platform (`times_large_seconds_shape0` to `_shape2`, `times_diff_shape0` to `_shape2`).

### Performance

Apple M4 Pro (arm64) and Intel i9-9980HK (x86_64), Apple clang 17, `-O2`, 4096 inputs per
row, best of 9 runs of about 15 ms, best of 5 rounds, each binary run twice (the second copy
of the old code, the A/A column, differs by at most about 3%), under an exclusive hold. The inputs
are read through an opaque pointer so a row cannot be hoisted out of its repetitions.
Nanoseconds per operation:

| row | M4 old | M4 new | ratio | i9 old | i9 new | ratio |
|---|---|---|---|---|---|---|
| `+= / -=` pair | 3.617 | 2.639 | 0.73 | 5.469 | 5.283 | 0.97 |
| `as_double()` | 0.572 | 0.569 | 0.99 | 1.132 | 1.123 | 0.99 |
| `now()` | 12.71 | 12.60 | 0.99 | 23.93 | 23.90 | 1.00 |
| `timespec_t * double` | 0.970 | 0.673 | 0.69 | 5.075 | 4.844 | 0.95 |
| `timespec_t / double` | 1.120 | 0.708 | 0.63 | 5.768 | 3.313 | 0.57 |
| `double < timespec_t` | 0.747 | 0.745 | 1.00 | 2.122 | 2.298 | 1.08 |
| `timespec_t(double)`, array of values in [0, 2^31) | 0.326 | 0.358 | 1.10 | 1.156 | 1.159 | 1.00 |
| `timespec_t(double)`, array of values in (-2^31, 0] | 0.327 | 0.358 | 1.10 | 1.170 | 1.150 | 0.98 |
| `timespec_t(double)`, each input from the previous result, positive | 11.68 | 8.91 | 0.76 | 11.89 | 10.48 | 0.88 |
| `timespec_t(double)`, each input from the previous result, negative | 11.76 | 8.92 | 0.76 | 11.96 | 10.56 | 0.88 |

The A/A spread in this run is 2% or less. `as_double()` and `now()` run code this change does not
touch. `+=` and `-=` are faster because `_adjust()` compares with integer constants, where
`1e9L` made every call an x87 operation on x86 and a double conversion on arm64; `*` and `/`
gain that and drop a second `_adjust()` after a constructor that already normalizes. The
constructor over an array of M4 values is 10% slower: the old loop vectorized because it did
not normalize a negative value (4 vector operations per pair of values, the borrow, that the
new one adds) and converted through `int`. On the i9 `double < timespec_t` is 8% slower: the
comparison reads the seconds first, which now wait for the nanosecond conversion that
produces the borrow.

The constructor has three shapes with the same value (`TIMES_CTOR_SHAPE`), chosen by
measuring the array row on both machines (ratio to the old code on the M4 and the i9): one
64-bit conversion and its inverse, the portable shape (1.10, 1.43); a `trunc` instruction
with 64-bit lanes, the arm64 default (1.09, 1.25); and a `trunc` with a 32-bit nanosecond
count, the x86 default with SSE4.1, which the compiler converts packed (1.19, 1.00). Three
other forms were measured and dropped. A `[[unlikely]]` branch for the borrow, the previous
version, keeps the loop from vectorizing (1.41, 1.39). A `floor`, which needs no borrow,
rounds a negative value to the nanosecond below, so `timespec_t(-0.8)` had `tv_nsec =
199999999` where the decimal says 200000000, for 214 of the 1107 decimals of one to three
digits that the truncation gives exactly. Converting the seconds as two 32-bit halves was
1.5 times slower on x86 for the array and 1.4 times slower for a chain.
