// The comparisons with a double on the left take their timespec_t by const reference, so a
// temporary and a const object compare: `0.5 < now()` builds. Before, the parameter was a
// non-const reference and only a named, non-const timespec_t bound to it.
//
// Oracle: the comparison of two timespec_t, which takes the same values through the member
// operators, and the plain double comparison of the same numbers.
//
// Build via CMake: cmake -B build && cmake --build build && ctest --test-dir build
#include <cstdio>

#include "check.h"
#include "times.h"


static timespec_t two() {
	return timespec_t(2.0);
}


static void test_temporaries() {
	CHECK(0.5 < now());                         // the wall clock is far past 0.5 s
	CHECK(!(1e12 < now()));                     // and not past 1e12 s
	CHECK(1e12 > now());
	CHECK(0.5 <= now());
	CHECK(1e12 >= now());
	CHECK(!(0.5 == now()));
	CHECK(0.5 != now());

	CHECK(1.0 < two());
	CHECK(1.0 <= two());
	CHECK(3.0 > two());
	CHECK(3.0 >= two());
	CHECK(2.0 == two());
	CHECK(2.5 != two());
	CHECK(!(2.0 < two()));
	CHECK(2.0 <= two());
	CHECK(2.0 >= two());
	CHECK(!(2.0 != two()));

	// A temporary built by arithmetic.
	CHECK(0.5 < timespec_t(1.0) + timespec_t(1.0));
	CHECK(3.0 == timespec_t(1.5) * 2.0);
	CHECK(0.25 == timespec_t(1.0) / 4.0);
	std::printf("times temporaries OK: a double compares with a timespec_t returned by value\n");
}


static void test_const_objects() {
	const timespec_t c(2.0);
	CHECK(1.0 < c);
	CHECK(1.0 <= c);
	CHECK(3.0 > c);
	CHECK(3.0 >= c);
	CHECK(2.0 == c);
	CHECK(2.5 != c);

	// Agreement with the member operators and the doubles, over a grid.
	for (int j = -8; j <= 8; ++j) {
		double d = j / 4.0;
		for (int k = -8; k <= 8; ++k) {
			const timespec_t t(k / 4.0);
			CHECK((d < t) == (timespec_t(d) < t));
			CHECK((d <= t) == (timespec_t(d) <= t));
			CHECK((d > t) == (timespec_t(d) > t));
			CHECK((d >= t) == (timespec_t(d) >= t));
			CHECK((d == t) == (timespec_t(d) == t));
			CHECK((d != t) == (timespec_t(d) != t));
			CHECK((d < t) == (j < k));
			CHECK((d == t) == (j == k));
		}
	}
	std::printf("times const objects OK: a double compares with a const timespec_t\n");
}


int main() {
	test_temporaries();
	test_const_objects();
	std::printf("all times comparison tests passed\n");
	return 0;
}
