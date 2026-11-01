// timespec_t(double), `*` and `/` against the code they replaced (test/old/times_old.h, frozen).
//
// Where the old code was defined and gave the right value, the new code returns it. The result
// classes, and what the test asserts on each:
//   A. 0 <= x < 2^31: the fields are identical.
//   B. a negative integer above -2^31: the fields are identical.
//   C. a negative value with a fraction of a nanosecond or more: the old constructor left
//      tv_nsec negative, the new one folds it into [0, 1e9). The fields differ (old != new),
//      the value does not: the old value normalized by one addition of zero is the new value.
//   D. |x| >= 2^31: the old code converted through int, which is undefined behavior (arm64
//      saturates, x86 gives INT_MIN). It is not executed here; test/large_seconds.cc checks the
//      new value against 128-bit integer arithmetic and std::chrono.
//   * and / with results in range: identical in every class, because the old code normalized
//     after the constructor.
//
// Build via CMake: cmake -B build && cmake --build build && ctest --test-dir build
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "check.h"
#include "old/times_old.h"
#include "times.h"

struct rng_t {
	std::uint64_t s;
	explicit rng_t(std::uint64_t seed) : s(seed) {}
	std::uint64_t next() {
		s ^= s >> 12;
		s ^= s << 25;
		s ^= s >> 27;
		return s * 0x2545F4914F6CDD1DULL;
	}
	double unit() { return static_cast<double>(next() >> 11) / 9007199254740992.0; }   // [0, 1)
};

static bool same_fields(const timespec_t& a, const times_old::timespec_t& b) {
	return a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec;
}


static void test_constructor() {
	rng_t rng(0xD1FF5ULL);
	const double two31 = 2147483648.0;

	// A and B.
	const double edges[] = {
		0.0, 0.5, 1.0, 1.5, 2.0, 1e-9, 1e-10, 0.999999999, 59.999999999, 1700000000.123456789,
		std::nextafter(two31, 0.0), 2147483647.0,
		-1.0, -2.0, -59.0, -2147483647.0,
	};
	for (double x : edges) {
		CHECK(same_fields(timespec_t(x), times_old::timespec_t(x)));
	}
	for (int i = 0; i < 200000; ++i) {
		double x = rng.unit() * two31;                            // A
		CHECK(same_fields(timespec_t(x), times_old::timespec_t(x)));
		double w = std::floor(rng.unit() * two31);                // A, whole seconds
		CHECK(same_fields(timespec_t(w), times_old::timespec_t(w)));
		CHECK(same_fields(timespec_t(-w), times_old::timespec_t(-w)));   // B
	}

	// C.
	long differ = 0;
	for (int i = 0; i < 200000; ++i) {
		double x = -(rng.unit() * two31);
		if (i % 5 == 0) x = -rng.unit();                          // between -1 and 0
		if (std::floor(x) == x) continue;
		times_old::timespec_t o(x);
		timespec_t n(x);
		CHECK(n.tv_nsec >= 0 && n.tv_nsec < 1000000000L);
		if (o.tv_nsec < 0) {
			++differ;
			CHECK(!same_fields(n, o));
			times_old::timespec_t folded = o + times_old::timespec_t();
			CHECK(same_fields(n, folded));
			CHECK(std::fabs(n.as_double() - o.as_double()) < 1e-6);
		} else {
			CHECK(same_fields(n, o));                             // a fraction below one nanosecond
		}
	}
	CHECK(differ > 100000);
	std::printf("times constructor OK: classes A and B identical, class C folded (%ld values)\n", differ);
}


static void test_scaling() {
	rng_t rng(0x5CA1EULL);
	for (int i = 0; i < 200000; ++i) {
		long sec = static_cast<long>(rng.next() % (1ULL << 28));
		long nsec = static_cast<long>(rng.next() % 1000000000ULL);
		timespec_t a;
		a.tv_sec = sec;
		a.tv_nsec = nsec;
		times_old::timespec_t o;
		o.tv_sec = sec;
		o.tv_nsec = nsec;

		double f = (rng.unit() * 8.0) - 4.0;                      // [-4, 4)
		CHECK(same_fields(a * f, o * f));
		if (std::fabs(f) >= 0.25) CHECK(same_fields(a / f, o / f));
		timespec_t b = a;
		times_old::timespec_t p = o;
		b *= f;
		p *= f;
		CHECK(same_fields(b, p));
	}
	std::printf("times scaling differential OK: * and / match the old results in range\n");
}


int main() {
	test_constructor();
	test_scaling();
	std::printf("all times differential tests passed\n");
	return 0;
}
