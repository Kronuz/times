// Smoke test for the standalone times library.
//
// Exercises times.h: timespec_t (a timespec subclass with set_now/as_double/
// clear), the free now() reading the realtime clock, the double <-> timespec_t
// conversion, the +/- arithmetic operators and the * / scalar-scaling operators
// with their nanosecond carry
// normalization, and the comparison operators. Time is read from the wall clock,
// so the timing checks use loose bounds to stay flake-free.
//
// Build via CMake: cmake -B build && cmake --build build && ctest --test-dir build
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "times.h"


// ---------------------------------------------------------------------------
// now() reads a real clock and never goes backwards. We take two readings with
// a tiny sleep between and assert the second is >= the first, and that the
// reading is a plausible absolute wall-clock time (well past the year 2000).
// ---------------------------------------------------------------------------

static void test_now_advances() {
	timespec_t t0 = now();

	// Sleep ~5ms so a second reading is observably later. Loose: we only assert
	// monotonic non-decrease, not a precise delta.
	struct timespec nap = { 0, 5 * 1000 * 1000 };  // 5,000,000 ns
	nanosleep(&nap, nullptr);

	timespec_t t1 = now();

	assert(t1 >= t0);            // the realtime clock did not run backwards
	assert(!(t1 < t0));          // and the inverse comparison agrees

	// A real wall-clock reading is far past 2000-01-01 (946684800 s). This pins
	// that now() is reading an absolute clock, not returning a zeroed struct.
	assert(t0.tv_sec > 946684800);

	std::printf("times now() OK: monotonic non-decrease, plausible absolute time\n");
}


// ---------------------------------------------------------------------------
// Elapsed time via subtraction. Sleep a known minimum and confirm the measured
// gap is at least that long (lower bound only) and not absurdly large.
// ---------------------------------------------------------------------------

static void test_elapsed() {
	timespec_t start = now();

	struct timespec nap = { 0, 20 * 1000 * 1000 };  // 20ms
	nanosleep(&nap, nullptr);

	timespec_t elapsed = now() - start;
	double secs = elapsed.as_double();

	// Lower bound: at least 10ms really elapsed (well under the 20ms slept, to
	// absorb timer coarseness). Upper bound: generous, just catches gross bugs.
	assert(secs >= 0.010);
	assert(secs < 5.0);

	std::printf("times elapsed OK: subtraction measured %.4f s after a 20ms sleep\n", secs);
}


// ---------------------------------------------------------------------------
// double <-> timespec_t conversion. The explicit double ctor splits whole
// seconds from the fractional nanosecond part; as_double() reassembles it.
// ---------------------------------------------------------------------------

static void test_double_conversion() {
	const double v = 3.25;  // 3 s + 0.25 s
	timespec_t ts(v);

	assert(ts.tv_sec == 3);
	// 0.25 s == 250,000,000 ns. Allow a small slop for the double->long cast.
	assert(std::llabs((long long)ts.tv_nsec - 250000000LL) <= 2);

	double back = ts.as_double();
	assert(std::fabs(back - v) < 1e-6);

	// Round number: an integral second has a zero nanosecond field.
	timespec_t whole(7.0);
	assert(whole.tv_sec == 7);
	assert(whole.tv_nsec == 0);
	assert(std::fabs(whole.as_double() - 7.0) < 1e-9);

	std::printf("times conversion OK: double <-> timespec_t round-trips\n");
}


// ---------------------------------------------------------------------------
// Arithmetic and nanosecond carry. Adding two values whose nanosecond parts
// overflow 1e9 must carry into tv_sec; subtracting back must borrow.
// ---------------------------------------------------------------------------

static void test_arithmetic_carry() {
	// 0.6 s + 0.7 s = 1.3 s. The nanosecond parts (6e8 + 7e8 = 1.3e9) overflow
	// one second, so _adjust() must carry: result is 1 s + 0.3 s.
	timespec_t a(0.6);
	timespec_t b(0.7);
	timespec_t sum = a + b;

	assert(sum.tv_sec == 1);
	assert(std::fabs(sum.as_double() - 1.3) < 1e-6);

	// Subtraction borrows the other way: 1.3 - 0.7 == 0.6.
	timespec_t diff = sum - b;
	assert(diff.tv_sec == 0);
	assert(std::fabs(diff.as_double() - 0.6) < 1e-6);

	// In-place += agrees with the binary operator.
	timespec_t acc(0.6);
	acc += b;
	assert(acc == sum);

	std::printf("times arithmetic OK: +/- carry and borrow across the 1e9 boundary\n");
}


// ---------------------------------------------------------------------------
// Comparison operators, including the free double-vs-timespec_t overloads.
// ---------------------------------------------------------------------------

static void test_comparisons() {
	timespec_t small(1.0);
	timespec_t large(2.0);
	timespec_t small_copy(1.0);

	assert(small < large);
	assert(small <= small_copy);
	assert(large > small);
	assert(large >= large);
	assert(small == small_copy);
	assert(small != large);

	// Free operators with a plain double on the left-hand side.
	assert(0.5 < large);
	assert(3.0 > large);
	assert(1.0 == small);
	assert(2.5 != large);

	std::printf("times comparisons OK: member and double-lhs operators agree\n");
}


// ---------------------------------------------------------------------------
// clear() zeroes the struct; the default ctor starts cleared.
// ---------------------------------------------------------------------------

static void test_clear() {
	timespec_t ts(9.5);
	assert(ts.tv_sec != 0 || ts.tv_nsec != 0);
	ts.clear();
	assert(ts.tv_sec == 0);
	assert(ts.tv_nsec == 0);

	timespec_t fresh;
	assert(fresh.tv_sec == 0);
	assert(fresh.tv_nsec == 0);

	std::printf("times clear() OK: cleared and default-constructed are zero\n");
}


// ---------------------------------------------------------------------------
// Scaling by a scalar. `*` and `/` multiply/divide the whole duration by a
// factor (operating on as_double()), with the nanosecond carry re-normalized.
// This is the physically meaningful scaling; it also pins that `/` divides
// (it used to be a copy of `*`).
// ---------------------------------------------------------------------------

static void test_scaling() {
	// Halving and tripling a duration.
	timespec_t half = timespec_t(1.5) / 2.0;   // 0.75 s
	assert(std::fabs(half.as_double() - 0.75) < 1e-6);

	timespec_t triple = timespec_t(1.5) * 3.0;  // 4.5 s
	assert(triple.tv_sec == 4);
	assert(std::fabs(triple.as_double() - 4.5) < 1e-6);

	// Scaling carries across the 1e9 boundary: 0.6 s * 2 = 1.2 s.
	timespec_t carried = timespec_t(0.6) * 2.0;
	assert(carried.tv_sec == 1);
	assert(std::fabs(carried.as_double() - 1.2) < 1e-6);

	// In-place forms agree with the binary operators.
	timespec_t v(2.0);
	v /= 4.0;                                    // 0.5 s
	assert(std::fabs(v.as_double() - 0.5) < 1e-6);

	// `/` really divides (not multiplies): dividing by 1 is identity, and
	// dividing by 2 is strictly smaller than the original.
	timespec_t original(3.0);
	assert(std::fabs((original / 1.0).as_double() - 3.0) < 1e-6);
	assert((original / 2.0).as_double() < original.as_double());

	std::printf("times scaling OK: * and / scale a duration by a scalar\n");
}


int main() {
	test_now_advances();
	test_elapsed();
	test_double_conversion();
	test_arithmetic_carry();
	test_comparisons();
	test_clear();
	test_scaling();
	std::printf("all times tests passed\n");
	return 0;
}
