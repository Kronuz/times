// Seconds beyond the range of int, and the normalization of a negative value.
//
// timespec_t(double) converted the whole seconds through int, so 3e9 s came back as
// 2147483647 (on arm64; on x86 the conversion is undefined and gives INT_MIN), and `*` and `/`,
// which build their result through that constructor, did too. A negative non-integral value
// left tv_nsec negative, so it compared unequal to the same instant produced by arithmetic.
//
// Oracles, none of them the implementation:
//  - a table of whole seconds written as decimal integers;
//  - values of the form a + b/4096, exact in a double, whose seconds are a and whose
//    nanoseconds are b * 1953125 / 8 in 64-bit integer arithmetic, truncated toward zero as
//    duration_cast does, and, where the count fits a double exactly, std::chrono::duration_cast;
//  - std::chrono::nanoseconds arithmetic for +, -, and the comparisons.
//
// Build via CMake: cmake -B build && cmake --build build && ctest --test-dir build
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "check.h"
#include "times.h"

using ns_t = std::chrono::nanoseconds;

static ns_t as_ns(const timespec_t& t) {
	return std::chrono::seconds(t.tv_sec) + ns_t(t.tv_nsec);
}

static bool normalized(const timespec_t& t) {
	return t.tv_nsec >= 0 && t.tv_nsec < 1000000000L;
}

static timespec_t make(long long sec, long nsec) {
	timespec_t t;
	t.tv_sec = static_cast<time_t>(sec);
	t.tv_nsec = nsec;
	return t;
}

struct rng_t {
	std::uint64_t s;
	explicit rng_t(std::uint64_t seed) : s(seed) {}
	std::uint64_t next() {
		s ^= s >> 12;
		s ^= s << 25;
		s ^= s >> 27;
		return s * 0x2545F4914F6CDD1DULL;
	}
};


static void test_whole_seconds() {
	const long long values[] = {
		0LL, 1LL, 59LL, 2147483646LL, 2147483647LL, 2147483648LL, 2147483649LL,
		4294967295LL, 4294967296LL, 3000000000LL, 4102444800LL /* 2100-01-01 */,
		1000000000000LL, 4503599627370496LL /* 2^52 */, 9007199254740992LL /* 2^53 */,
		4611686018427387904LL /* 2^62 */,
		-1LL, -59LL, -2147483647LL, -2147483648LL, -2147483649LL, -3000000000LL,
		-1000000000000LL, -4611686018427387904LL,
	};
	for (long long v : values) {
		timespec_t ts(static_cast<double>(v));
		CHECK(static_cast<long long>(ts.tv_sec) == v);
		CHECK(ts.tv_nsec == 0);
		CHECK(ts.as_double() == static_cast<double>(v));
	}

	timespec_t t(3e9);
	CHECK(t.tv_sec == 3000000000LL);
	CHECK(t.tv_nsec == 0);

	// The Unix time after January 2038, with a fraction.
	timespec_t u(2147483648.5);
	CHECK(u.tv_sec == 2147483648LL);
	CHECK(u.tv_nsec == 500000000L);
	std::printf("times whole seconds OK: seconds past int and below it come back exact\n");
}


// Values a + b/4096 with |a| < 2^40: exact in a double, so the expected fields are exact.
static void test_exact_fractions() {
	rng_t rng(0x71AE5ULL);
	for (int i = 0; i < 200000; ++i) {
		long long a = static_cast<long long>(rng.next() % (1ULL << 41)) - (1LL << 40);
		long long b = static_cast<long long>(rng.next() % 4096);
		double x = static_cast<double>(a) + static_cast<double>(b) / 4096.0;

		// x = a + b/4096 with 0 <= b < 4096, so the whole seconds (rounded down) are a, and the
		// nanoseconds of the fraction are b * 1e9 / 4096 = b * 1953125 / 8. The count truncates
		// toward zero, as duration_cast does: down for x >= 0, up (toward x) for x < 0.
		long long frac = b * 1953125;                         // under 2^33
		long long expect_nsec = a >= 0 ? frac / 8 : (frac + 7) / 8;

		timespec_t ts(x);
		CHECK(static_cast<long long>(ts.tv_sec) == a);
		CHECK(static_cast<long long>(ts.tv_nsec) == expect_nsec);
		CHECK(normalized(ts));

		// Where the nanosecond count is exact in a double (|a| < 2^20), std::chrono agrees.
		if (a > -(1LL << 20) && a < (1LL << 20)) {
			auto c = std::chrono::duration_cast<ns_t>(std::chrono::duration<double>(x));
			CHECK(as_ns(ts) == c);
		}
	}
	std::printf("times fractions OK: a + b/4096 gives the exact seconds and nanoseconds\n");
}


// Every multiple of 1/8 from -20 to 20: fields, the order, and equality with arithmetic.
static void test_negative_values() {
	for (int k = -160; k <= 160; ++k) {
		double x = k / 8.0;
		timespec_t ts(x);
		long long sec = k >= 0 ? k / 8 : -((-k + 7) / 8);   // floor
		long nsec = static_cast<long>((k - sec * 8) * 125000000LL);
		CHECK(static_cast<long long>(ts.tv_sec) == sec);
		CHECK(ts.tv_nsec == nsec);
		// The same instant by arithmetic: k/8 s as k one-eighth-second steps.
		timespec_t eighth = make(0, 125000000);
		timespec_t acc;
		for (int i = 0; i < (k < 0 ? -k : k); ++i) {
			if (k < 0) acc -= eighth; else acc += eighth;
		}
		CHECK(acc == ts);
		CHECK(!(acc != ts));
		CHECK(x == acc);                                  // the double-first operators agree
		for (int j = -160; j <= 160; ++j) {
			timespec_t other(j / 8.0);
			CHECK((ts < other) == (k < j));
			CHECK((ts <= other) == (k <= j));
			CHECK((ts == other) == (k == j));
		}
	}
	// The cases that were wrong: a negative fraction was (0, -5e8) and (-1, -5e8).
	CHECK(timespec_t(-0.5) == timespec_t(0.5) - timespec_t(1.0));
	CHECK(timespec_t(-1.5) == timespec_t(0.5) - timespec_t(2.0));
	CHECK(timespec_t(-1.25) + timespec_t(1.25) == timespec_t());
	std::printf("times negative values OK: tv_nsec stays in [0, 1e9) and the values order and add\n");
}


// +, - and the comparisons on seconds past int, against std::chrono::nanoseconds.
static void test_arithmetic_beyond_int() {
	rng_t rng(0xA11CEULL);
	for (int i = 0; i < 100000; ++i) {
		long long sa = static_cast<long long>(rng.next() % 8000000001ULL) - 4000000000LL;
		long long sb = static_cast<long long>(rng.next() % 8000000001ULL) - 4000000000LL;
		timespec_t a = make(sa, static_cast<long>(rng.next() % 1000000000ULL));
		timespec_t b = make(sb, static_cast<long>(rng.next() % 1000000000ULL));
		if (i % 7 == 0) b = make(sb, a.tv_nsec);       // equal nanoseconds
		if (i % 11 == 0) b = a;                        // equal values

		timespec_t sum = a + b;
		timespec_t diff = a - b;
		CHECK(normalized(sum));
		CHECK(normalized(diff));
		CHECK(as_ns(sum) == as_ns(a) + as_ns(b));
		CHECK(as_ns(diff) == as_ns(a) - as_ns(b));
		timespec_t acc = a;
		acc += b;
		CHECK(acc == sum);
		acc -= b;
		CHECK(acc == a);

		CHECK((a < b) == (as_ns(a) < as_ns(b)));
		CHECK((a <= b) == (as_ns(a) <= as_ns(b)));
		CHECK((a > b) == (as_ns(a) > as_ns(b)));
		CHECK((a >= b) == (as_ns(a) >= as_ns(b)));
		CHECK((a == b) == (as_ns(a) == as_ns(b)));
		CHECK((a != b) == (as_ns(a) != as_ns(b)));
	}
	std::printf("times arithmetic OK: + - and the comparisons agree with std::chrono beyond int\n");
}


// * and / go through the constructor: whole seconds past int survive.
static void test_scaling_beyond_int() {
	timespec_t six = timespec_t(3e9) * 2.0;
	CHECK(six.tv_sec == 6000000000LL && six.tv_nsec == 0);
	timespec_t three = timespec_t(6e9) / 2.0;
	CHECK(three.tv_sec == 3000000000LL && three.tv_nsec == 0);
	timespec_t same = timespec_t(3e9) * 1.0;
	CHECK(same == timespec_t(3e9));
	timespec_t five = timespec_t(1e10) * 0.5;
	CHECK(five.tv_sec == 5000000000LL && five.tv_nsec == 0);
	timespec_t neg = timespec_t(3e9) * -2.0;
	CHECK(neg.tv_sec == -6000000000LL && neg.tv_nsec == 0);
	timespec_t big = timespec_t(0.25) * 4e9;
	CHECK(big.tv_sec == 1000000000LL && big.tv_nsec == 0);
	timespec_t v(5e9);
	v *= 3.0;
	CHECK(v.tv_sec == 15000000000LL);
	v /= 5.0;
	CHECK(v.tv_sec == 3000000000LL);
	// A negative result stays normalized.
	timespec_t back = timespec_t(0.25) * -1.0;
	CHECK(back.tv_sec == -1 && back.tv_nsec == 750000000L);
	std::printf("times scaling OK: * and / keep seconds past int\n");
}


// A deadline set from the clock reaches past 2038 without wrapping.
static void test_deadline_after_2038() {
	timespec_t t = now();
	timespec_t later = t + timespec_t(1e9);                 // about 31.7 years on
	CHECK(later.tv_sec == t.tv_sec + 1000000000LL);
	CHECK(t < later);
	timespec_t round = timespec_t(later.as_double());       // through a double and back
	CHECK(std::fabs((round - later).as_double()) < 1e-6);   // a double holds ~0.5 us at 2.8e9 s
	std::printf("times deadline OK: now() + 1e9 s past 2038 round-trips through a double\n");
}


int main() {
	test_whole_seconds();
	test_exact_fractions();
	test_negative_values();
	test_arithmetic_beyond_int();
	test_scaling_beyond_int();
	test_deadline_after_2038();
	std::printf("all times large-seconds tests passed\n");
	return 0;
}
