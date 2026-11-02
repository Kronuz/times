# Changelog

All notable changes to `times`. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); the library has no
releases, so entries are dated.

Upgrading a pin from before 2026-10-30: [the consumer upgrade note](https://github.com/Kronuz/cpp-libraries/blob/main/docs/UPGRADING-2026-10.md#times) lists what changes for callers, how to find affected inputs, the commits not to pin, and the speedup.

## 2026-11-01, the constructor for one value

### Changed

The final gate (START `666dfdd`) put two `timespec_t(double)` rows of a scalar loop on the i9 at 0.82 to
0.83 ("chain through `tv_nsec`"), `double < timespec_t` at 0.93 and the constructor over an array at 0.92 on
the M4. The cause of the first two is not a latency chain: the compare with `LONG_MAX` that the row used to
carry the dependence compiles to `cmp; je`, a predicted branch, so the row timed the throughput of a loop
that builds one value per iteration. That is what the x86 shape got wrong: its 32-bit nanosecond count, which
x86 converts packed over an array, costs two sign extensions (`movslq`) per construction, 15 to 18% on a loop
of single constructions. A real chain through `tv_nsec` (`x = p[i] + tv_nsec * 0.0`) was already 11 to 13%
faster than START on the i9 and 33% on the M4.

- **The default shape on x86 with SSE4.1 is the 64-bit one**, the shape arm64 already had (`trunc`, a 64-bit
  nanosecond count, the borrow as `ns >> 63`); the seconds come from `(long long)other`, which does not wait for
  the `trunc`. The 32-bit shape stays selectable (`TIMES_CTOR_SHAPE=1`), with the same seconds.
- **Not kept.** Unsigned arithmetic on the 32-bit shape (no sign extensions) compiles to the same code as the
  signed one but one `shr` and one `sub` longer than the 64-bit shape: scalar rows 0.93 to 0.95 of START, array
  1.18. A ternary borrow is 0.69 on the i9 array, and a multiplication by 1e9 for the borrow compiles to the same
  code. The portable shape is 0.69 there too.

Ratio START / new of the gate's micro-benchmark (the loop of single constructions is also in `bench/bench.cc`), one
strict hold per machine, four rounds of START, new, START, the A/A within 1.4% (above 1 is faster):

| row | M4 Pro before | M4 Pro after | i9 before | i9 after |
| --- | --- | --- | --- | --- |
| loop of single constructions, positive | 1.00 | 1.00 | 0.85 | 1.00 |
| loop of single constructions, negative | 1.00 | 0.99 | 0.82 | 0.98 |
| chain through `tv_nsec` (data), positive | 1.34 | 1.34 | 1.11 | 1.16 |
| chain through `as_double()`, positive | 1.22 | 1.25 | 1.09 | 1.06 |
| `double < timespec_t` | 1.01 | 1.02 | 0.93 | 1.02 |
| `timespec_t * double` | 1.46 | 1.45 | 1.50 | 1.14 |
| `timespec_t / double` | 1.64 | 1.60 | 1.70 | 1.16 |
| array of values in [0, 2^31) | 0.92 | 0.92 | 1.00 | 0.89 |
| array of values in (-2^31, 0] | 0.92 | 0.92 | 1.03 | 0.90 |

Rows left under 0.97, and why. On the M4 the array is 0.92 with every shape tried (six, from the 64-bit
conversion to a ternary borrow): the old loop vectorized with 9 vector instructions per pair of values, the new
one has the same count (`frintz`, two `fcvtzs`, the borrow's `cmlt`, `and`, `add` and `ssra`) and runs at 0.20
cycles more per pair; the borrow is what the correctness fix (a negative fraction folds into `tv_nsec`) adds. On the i9 the array is 0.89 because the
compiler converts the 64-bit shape one value at a time, where START's x87 loop is already scalar, and `*` and `/`
lose part of their gain for the same reason (1.5 and 1.7 to 1.14 and 1.16, still faster than START). The 32-bit
shape has the array at 1.18 and the scalar rows at 0.82 to 0.85; no shape has both.

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
- `bench/`: the benchmark rows of the Performance section, `abcmp.py`, and `old/times.h` (the
  header of `d762ec6`, the old side of the rows), behind `TIMES_BUILD_BENCH`. CI builds them and
  runs each binary once.

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

The rows are `bench/bench.cc`; build it with `-DTIMES_BUILD_BENCH=ON` and run
`python3 bench/abcmp.py build 5` (`AGENTS.md`, "Verification and benchmarks"). The A/A spread in
this run is 2% or less. `as_double()` and `now()` run code this change does not
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
