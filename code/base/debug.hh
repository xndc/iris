#pragma once
#include <loguru.hpp>
#include "base/base.hh"

// Marks that the current location is not supposed to be reachable. In debug builds, triggers an
// assertion failure if reached. In release builds, reaching this is undefined behaviour.
#if COMPILER_HAS_BUILTIN(__builtin_unreachable)
	#define AssertUnreachable() do { assert(!("Unreachable")); __builtin_unreachable(); } while(0)
#else
	#define AssertUnreachable() do { assert(!("Unreachable")); abort(); } while(0)
#endif

void InitDebugSystem(int argc, char* argv[]);
