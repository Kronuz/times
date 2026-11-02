/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 * Copyright (c) 2015-2018 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

// HAVE_CLOCK_GETTIME comes from this library's own CMake feature-detection
// (times_config.h), generated for a standalone build. A consumer that compiles
// times.cc into its own build (e.g. via FetchContent populate, without running
// this library's CMakeLists) will not have times_config.h; it should define
// HAVE_CLOCK_GETTIME itself. Guard the include so both paths work.
#if __has_include("times_config.h")
#include "times_config.h"          // for HAVE_CLOCK_GETTIME
#endif

#include <sys/time.h>
#include <time.h>                  // for clock_gettime, CLOCK_REALTIME

// Which way timespec_t(double) splits a double into seconds and nanoseconds. All three give the
// same value; they differ in what the compiler can do with it. 2: one trunc instruction and 64-bit
// arithmetic, the default on x86 with SSE4.1 and on arm64 (the least work for one construction,
// which is what a call site does); 1: a 32-bit nanosecond count, which the compiler converts packed
// in a loop over an array on x86, at two sign extensions more for one construction (a scalar loop
// runs 15% slower than the other two); 0: a 64-bit conversion and its inverse, for any other
// target. Define it to force a shape.
#ifndef TIMES_CTOR_SHAPE
#if defined(__SSE4_1__) || defined(__aarch64__)
#define TIMES_CTOR_SHAPE 2
#else
#define TIMES_CTOR_SHAPE 0
#endif
#endif


#ifdef __linux__

#include <time.h>                  // for clock_gettime, CLOCK_REALTIME
#include <unistd.h>

#endif /*__linux__*/

#ifdef __APPLE__

#ifndef HAVE_CLOCK_GETTIME

#include <errno.h>
#include <mach/clock.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 4
#define CLOCK_THREAD_CPUTIME_ID 14
#define CLOCK_PROCESS_CPUTIME_ID 15


using clockid_t = int;

int clock_gettime(clockid_t clk_id, struct timespec *tp);

#endif /* HAVE_CLOCK_GETTIME */

#endif /* __APPLE__ */


#define _timespec_cmp(tsp0, cmp, tsp1) \
	(((tsp0)->tv_sec == (tsp1)->tv_sec) ? \
		((tsp0)->tv_nsec cmp (tsp1)->tv_nsec) : \
		((tsp0)->tv_sec cmp (tsp1)->tv_sec));


struct timespec_t : public timespec {
	timespec_t() {
		clear();
	}

	// The seconds, truncated toward zero, go into the 64-bit tv_sec and the fraction, truncated
	// to nanoseconds, into tv_nsec; a negative value then borrows one second, so tv_nsec is in
	// [0, 1e9). The fraction is scaled in double on every platform, so 0.3 s is 300000000 ns
	// everywhere. `other` must be finite and within the range of time_t, as for a duration_cast.
	explicit timespec_t(double other) {
#if TIMES_CTOR_SHAPE == 1
		const double w = __builtin_trunc(other);
		const int ns = (int)((other - w) * 1e9);
		const int borrow = ns >> 31;
		tv_sec = (long long)other + borrow;
		tv_nsec = ns + (borrow & 1000000000);
#elif TIMES_CTOR_SHAPE == 2
		const double w = __builtin_trunc(other);
		const long long ns = (long long)((other - w) * 1e9);
		const long long borrow = ns >> 63;
		tv_sec = (long long)other + borrow;
		tv_nsec = ns + (borrow & 1000000000LL);
#else
		const long long whole = (long long)other;
		const long long ns = (long long)((other - (double)whole) * 1e9);
		const long long borrow = ns >> 63;
		tv_sec = whole + borrow;
		tv_nsec = ns + (borrow & 1000000000LL);
#endif
	}

	inline void clear() noexcept {
		tv_nsec = 0;
		tv_sec = 0;
	}

	inline void set_now() {
		clock_gettime(CLOCK_REALTIME, this);
	}

	inline double as_double() const noexcept {
		return (double)tv_sec + ((double)tv_nsec / 1e9L);
	}

	inline timespec_t& operator=(const timespec_t& other) {
		tv_nsec = other.tv_nsec;
		tv_sec = other.tv_sec;
		return *this;
	}

	inline void operator-=(const timespec_t& other) {
		tv_nsec -= other.tv_nsec;
		tv_sec -= other.tv_sec;
		_adjust();
	}

	inline timespec_t operator-(const timespec_t& other) const {
		timespec_t ts = *this;
		ts -= other;
		return ts;
	}

	inline void operator+=(const timespec_t& other) {
		tv_nsec += other.tv_nsec;
		tv_sec += other.tv_sec;
		_adjust();
	}

	inline timespec_t operator+(const timespec_t& other) const {
		timespec_t ts = *this;
		ts += other;
		return ts;
	}

	// Scale a duration by a scalar factor. `*` and `/` operate on the whole
	// value (via as_double()), not on the sec/nsec fields separately, so the
	// result is a physically meaningful multiple/fraction of the duration; the
	// constructor leaves the nanosecond field in [0, 1e9).
	inline void operator*=(double factor) {
		*this = timespec_t(as_double() * factor);
	}

	inline timespec_t operator*(double factor) const {
		timespec_t ts = *this;
		ts *= factor;
		return ts;
	}

	inline void operator/=(double factor) {
		*this = timespec_t(as_double() / factor);
	}

	inline timespec_t operator/(double factor) const {
		timespec_t ts = *this;
		ts /= factor;
		return ts;
	}

	inline bool operator<(const timespec_t& other) const {
		return _timespec_cmp(this, <, &other);
	}

	inline bool operator<=(const timespec_t& other) const {
		return _timespec_cmp(this, <=, &other);
	}

	inline bool operator>(const timespec_t& other) const {
		return _timespec_cmp(this, >, &other);
	}

	inline bool operator>=(const timespec_t& other) const {
		return _timespec_cmp(this, >=, &other);
	}

	inline bool operator==(const timespec_t& other) const {
		return _timespec_cmp(this, ==, &other);
	}

	inline bool operator!=(const timespec_t& other) const {
		return _timespec_cmp(this, !=, &other);
	}

private:
	inline void _adjust() {
		while (tv_nsec < 0) {
			tv_nsec += 1000000000L;
			tv_sec--;
		}
		while (tv_nsec >= 1000000000L) {
			tv_nsec -= 1000000000L;
			tv_sec++;
		}
	}
};


inline timespec_t now() {
	timespec_t ts;
	ts.set_now();
	return ts;
}


inline bool operator<(double dt0, const timespec_t& ts1) {
	return timespec_t(dt0) < ts1;
}


inline bool operator<=(double dt0, const timespec_t& ts1) {
	return timespec_t(dt0) <= ts1;
}


inline bool operator>(double dt0, const timespec_t& ts1) {
	return timespec_t(dt0) > ts1;
}


inline bool operator>=(double dt0, const timespec_t& ts1) {
	return timespec_t(dt0) >= ts1;
}


inline bool operator==(double dt0, const timespec_t& ts1) {
	return timespec_t(dt0) == ts1;
}


inline bool operator!=(double dt0, const timespec_t& ts1) {
	return timespec_t(dt0) != ts1;
}
