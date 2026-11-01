// CHECK(cond) for the tests: unlike assert(), it stays live under NDEBUG (a Release build).
#pragma once

#include <cstdio>
#include <cstdlib>

#define CHECK(...) \
	do { \
		if (!(__VA_ARGS__)) { \
			std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #__VA_ARGS__); \
			std::abort(); \
		} \
	} while (0)
