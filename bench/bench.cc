// Benchmark rows for timespec_t in times.h. The same source is built three times: against the
// library (times_bench_new) and against the header as it was at d762ec6, before the whole seconds
// went through time_t, in bench/old/times.h, byte for byte (times_bench_old; times_bench_old2 is a
// second copy of it, whose ratio to the first is the A/A spread of the run). It prints
// "row ns/op" lines; bench/abcmp.py runs the binaries back to back and pairs them.
//
// Every row reads its input through a pointer the compiler cannot see through (launder), so it
// cannot hoist the loop out of the repetition and measures the loop, not one pass of it.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "times.h"

namespace {

using clk = std::chrono::steady_clock;

struct rng_t {
	std::uint64_t s = 0x9E3779B97F4A7C15ULL;
	std::uint64_t next() {
		s ^= s >> 12;
		s ^= s << 25;
		s ^= s >> 27;
		return s * 0x2545F4914F6CDD1DULL;
	}
	double unit() { return static_cast<double>(next() >> 11) / 9007199254740992.0; }   // [0, 1)
};

template <typename T>
inline void keep(const T& v) {
	asm volatile("" : : "r,m"(v) : "memory");
}

template <typename T>
inline T* hide(T* p) {
	asm volatile("" : "+r"(p));
	return p;
}

// A macro, not a function named launder: std::launder is found by argument-dependent lookup
// for std types and makes the call ambiguous.
#define launder(p) hide(p)

// Best of 9 runs of f(), in nanoseconds per operation; each run is repeated to take about 15 ms.
template <typename F>
__attribute__((noinline)) double bench(F&& f, std::size_t ops) {
	f();
	auto t0 = clk::now();
	f();
	double once = std::chrono::duration<double>(clk::now() - t0).count();
	int reps = once > 0 ? static_cast<int>(0.015 / once) + 1 : 1000;
	double best = 1e30;
	for (int r = 0; r < 9; ++r) {
		auto s = clk::now();
		for (int i = 0; i < reps; ++i) f();
		double e = std::chrono::duration<double>(clk::now() - s).count();
		best = std::min(best, e * 1e9 / (static_cast<double>(reps) * static_cast<double>(ops)));
	}
	return best;
}

void row(const char* name, double ns) {
	std::printf("%-44s %8.3f\n", name, ns);
}

const std::size_t N = 4096;

}  // namespace

int main() {
	rng_t rng;
	std::vector<double> positive(N), negative(N), factors(N);
	std::vector<timespec_t> values(N);
	for (std::size_t i = 0; i < N; ++i) {
		positive[i] = rng.unit() * 2147483647.0;
		negative[i] = -rng.unit() * 2147483647.0;
		factors[i] = 0.5 + rng.unit();
		values[i].tv_sec = static_cast<long>(rng.next() % 1000000000ULL);
		values[i].tv_nsec = static_cast<long>(rng.next() % 1000000000ULL);
	}
	row("timespec_t(double) [0, 2^31)", bench([&] {
		const double* p = launder(positive.data());
		long s = 0;
		for (std::size_t i = 0; i < N; ++i) { timespec_t t(p[i]); s += t.tv_sec + t.tv_nsec; }
		keep(s);
	}, N));
	row("timespec_t(double) (-2^31, 0]", bench([&] {
		const double* p = launder(negative.data());
		long s = 0;
		for (std::size_t i = 0; i < N; ++i) { timespec_t t(p[i]); s += t.tv_sec + t.tv_nsec; }
		keep(s);
	}, N));
	// Each value depends on the previous result, so the loop cannot be vectorized: the cost of one
	// construction in scalar code.
	row("timespec_t(double) dependent chain, positive", bench([&] {
		double d = launder(positive.data())[0];
		long s = 0;
		for (std::size_t i = 0; i < N; ++i) {
			timespec_t t(d);
			s += t.tv_sec ^ t.tv_nsec;
			d = static_cast<double>(s & 0xFFFFF) * 0.37 + 0.5;
		}
		keep(s);
	}, N));
	row("timespec_t(double) dependent chain, negative", bench([&] {
		double d = launder(negative.data())[0];
		long s = 0;
		for (std::size_t i = 0; i < N; ++i) {
			timespec_t t(d);
			s += t.tv_sec ^ t.tv_nsec;
			d = -static_cast<double>(s & 0xFFFFF) * 0.37 - 0.5;
		}
		keep(s);
	}, N));
	// Independent values, one construction per iteration, both fields kept in registers by an empty asm: the
	// throughput of a call site that builds one value. The first two rows sum the fields, which lets the
	// compiler vectorize the loop (the old code's loop did not vectorize on x86 and did on arm64).
	row("timespec_t(double) scalar loop, positive", bench([&] {
		const double* p = launder(positive.data());
		long s = 0;
		for (std::size_t i = 0; i < N; ++i) {
			timespec_t t(p[i]);
			long sec = t.tv_sec, ns = t.tv_nsec;
			asm volatile("" : "+r"(sec), "+r"(ns));
			s += sec + ns;
		}
		keep(s);
	}, N));
	row("timespec_t(double) scalar loop, negative", bench([&] {
		const double* p = launder(negative.data());
		long s = 0;
		for (std::size_t i = 0; i < N; ++i) {
			timespec_t t(p[i]);
			long sec = t.tv_sec, ns = t.tv_nsec;
			asm volatile("" : "+r"(sec), "+r"(ns));
			s += sec + ns;
		}
		keep(s);
	}, N));
	row("timespec_t * double", bench([&] {
		const timespec_t* v = launder(values.data());
		const double* f = launder(factors.data());
		long s = 0;
		for (std::size_t i = 0; i < N; ++i) { timespec_t t = v[i] * f[i]; s += t.tv_sec + t.tv_nsec; }
		keep(s);
	}, N));
	row("timespec_t / double", bench([&] {
		const timespec_t* v = launder(values.data());
		const double* f = launder(factors.data());
		long s = 0;
		for (std::size_t i = 0; i < N; ++i) { timespec_t t = v[i] / f[i]; s += t.tv_sec + t.tv_nsec; }
		keep(s);
	}, N));
	row("timespec_t += / -= pair", bench([&] {
		const timespec_t* v = launder(values.data());
		timespec_t acc;
		for (int r = 0; r < 4; ++r) for (std::size_t i = 0; i < N; ++i) { acc += v[i]; acc -= v[0]; }
		keep(acc.tv_nsec);
	}, N * 4));
	row("as_double()", bench([&] {
		const timespec_t* v = launder(values.data());
		double s = 0;
		for (std::size_t i = 0; i < N; ++i) s += v[i].as_double();
		keep(s);
	}, N));
	row("double < timespec_t", bench([&] {
		const double* p = launder(positive.data());
		timespec_t* v = launder(values.data());   // non-const: the old operator takes a timespec_t&
		std::size_t c = 0;
		for (std::size_t i = 0; i < N; ++i) c += (p[i] < v[i]);
		keep(c);
	}, N));
	row("now()", bench([&] {
		long s = 0;
		for (std::size_t i = 0; i < N / 16; ++i) { auto t = now(); s += t.tv_nsec; }
		keep(s);
	}, N / 16));
	return 0;
}
