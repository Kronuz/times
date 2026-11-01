// timespec_t exactly as times.h had it before the whole seconds went through time_t
// (d762ec6), minus the clock reads and the comparisons, renamed times_old::timespec_t.
// Frozen: test/diff.cc compares the library against it. Do not edit.
#pragma once

#include <time.h>

namespace times_old {

struct timespec_t : public timespec {
	timespec_t() {
		clear();
	}

	explicit timespec_t(double other) {
		tv_nsec = (int)((other - (int)other) * 1e9L);
		tv_sec = (int)other;
	}

	inline void clear() noexcept {
		tv_nsec = 0;
		tv_sec = 0;
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

	inline void operator*=(double factor) {
		*this = timespec_t(as_double() * factor);
		_adjust();
	}

	inline timespec_t operator*(double factor) const {
		timespec_t ts = *this;
		ts *= factor;
		return ts;
	}

	inline void operator/=(double factor) {
		*this = timespec_t(as_double() / factor);
		_adjust();
	}

	inline timespec_t operator/(double factor) const {
		timespec_t ts = *this;
		ts /= factor;
		return ts;
	}

private:
	inline void _adjust() {
		while (tv_nsec < 0) {
			tv_nsec += 1e9L;
			tv_sec--;
		}
		while (tv_nsec  >= 1e9L) {
			tv_nsec -= 1e9L;
			tv_sec++;
		}
	}
};

}  // namespace times_old
