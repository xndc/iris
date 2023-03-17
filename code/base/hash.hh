#pragma once
#include "base/base.hh"

// 64-bit FNV hash parameters. Reference:
// https://en.wikipedia.org/wiki/Fowler-Noll-Vo_hash_function
static constexpr uint64_t FNV_BASIS = 14695981039346656037ULL;
static constexpr uint64_t FNV_PRIME = 1099511628211ULL;

// Compute a 64-bit string hash using the FNV-1a algorithm.
static FORCEINLINE constexpr uint64_t Hash64(const char* str) {
	uint64_t hash = FNV_BASIS;
	if (ExpectTrue(str)) {
		while (*str != '\0') {
			hash ^= *str++;
			hash *= FNV_PRIME;
		}
	}
	return hash;
}

// Compute a 64-bit hash from a sized buffer using the FNV-1a algorithm.
static FORCEINLINE constexpr uint64_t Hash64(const void* buffer, size_t bytes) {
	uint64_t hash = FNV_BASIS;
	if (ExpectTrue(buffer)) {
		const char* cbuffer = static_cast<const char*>(buffer);
		while (bytes--) {
			hash ^= *cbuffer++;
			hash *= FNV_PRIME;
		}
	}
	return hash;
}

// Compute a 64-bit hash from a fixed-size object using the FNV-1a algorithm.
// Templated "hasher" version for use with STL containers. Note that the output may be 32-bit.
struct Hash64T {
	template<typename T> FORCEINLINE size_t operator()(const T& x) const {
		return static_cast<size_t>(Hash64(reinterpret_cast<const char*>(&x)));
	}
};

// Check that Hash64 is evaluated at compile-time.
StaticAssert(Hash64("X") == 12638249872718450023ULL);
StaticAssert(Hash64(nullptr) == FNV_BASIS);
