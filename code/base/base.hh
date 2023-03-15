#pragma once

/***************************************************************************************************
 * Basic Utility Types, Functions and Macros
 **************************************************************************************************/

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <atomic>
#include <initializer_list>
#include <utility>

/***************************************************************************************************
 * Shims for functionality missing on some compilers. References:
 * https://en.cppreference.com/w/cpp/compiler_support for C++ compiler support tables
 * https://en.cppreference.com/w/cpp/feature_test for info about C++ feature testing
 * https://gcc.gnu.org/projects/cxx-status.html for GCC version/feature mappings
 * https://learn.microsoft.com/en-us/cpp/preprocessor/predefined-macros for MSVC version info
 **************************************************************************************************/

// Checks if this compiler has support for a given C++ version, e.g. COMPILER_HAS_CPP(200704L).
// Note that on MSVC, your program should be compiled with /Zc:__cplusplus and e.g. /std:c++14 for
// this macro to work. See https://learn.microsoft.com/en-us/cpp/build/reference/zc-cplusplus
#if defined(__cplusplus)
	#define COMPILER_HAS_CPP(version) (__cplusplus >= version)
#else
	#define COMPILER_HAS_CPP(version) 0
#endif

// Checks if the compiler is GCC [major].[minor] or newer. False if the compiler is not GCC.
#if defined(__GNUC_PREREQ__)
	#define COMPILER_IS_GCC(major, minor) __GNUC_PREREQ__(major, minor)
#elif defined(__GNUC__) && defined(__GNUC_MINOR__)
	#define COMPILER_IS_GCC(major, minor) ((__GNUC__ << 16) + (__GNUC_MINOR__) >= ((major) << 16) + (minor))
#else
	#define COMPILER_IS_GCC(major, minor) 0
#endif

// Checks if the compiler is MSVC [version] or newer. False if the compiler is not MSVC.
#if defined(_MSC_VER)
	#define COMPILER_IS_MSVC(version) (_MSC_VER >= version)
#else
	#define COMPILER_IS_MSVC(version) 0
#endif

// Checks if the compiler has a builtin with the given name.
#if defined(__has_builtin)
	#define COMPILER_HAS_BUILTIN(x) __has_builtin(x)
#else
	#define COMPILER_HAS_BUILTIN(x) ((defined(COMPILER_HAS_BUILTIN ## x) && COMPILER_HAS_BUILTIN ## x) || defined(x))
	#define COMPILER_HAS_BUILTIN__builtin_expect COMPILER_IS_GCC(3, 0) // https://gcc.gnu.org/gcc-3.0/features.html
#endif

// Checks if the compiler supports a C++ attribute [[x]] with the given name.
#if defined(__has_cpp_attribute)
	#define COMPILER_HAS_CPP_ATTRIB(x) __has_cpp_attribute(x)
#else
	#define COMPILER_HAS_CPP_ATTRIB(x) (defined(COMPILER_HAS_CPP_ATTRIB_ ## x) && COMPILER_HAS_CPP_ATTRIB_ ## x)
	#define COMPILER_HAS_CPP_ATTRIB_noreturn COMPILER_HAS_CPP(200809L)
	#define COMPILER_HAS_CPP_ATTRIB_nodiscard COMPILER_HAS_CPP(201603L)
#endif

// Checks if the compiler supports an __attribute__(x) with the given name.
#if defined(__has_attribute)
	#define COMPILER_HAS_GCC_ATTRIB(x) __has_attribute(x)
#else
	#define COMPILER_HAS_GCC_ATTRIB(x) (defined(COMPILER_HAS_GCC_ATTRIB_ ## x) && COMPILER_HAS_GCC_ATTRIB_ ## x)
	#define COMPILER_HAS_GCC_ATTRIB_fallthrough COMPILER_IS_GCC(7, 1) // https://gcc.gnu.org/gcc-7/changes.html
#endif

// Hints to the compiler that an expression is likely to have a particular value.
#if COMPILER_HAS_BUILTIN(__builtin_expect)
	#define ExpectEqual(expr, expected) __builtin_expect(expr, expected)
#else
	#define ExpectEqual(expr, expected) expr
#endif

// Hints that a branch or loop condition is likely to be true or false.
#define ExpectTrue(cond)  ExpectEqual(!!(cond), 1)
#define ExpectFalse(cond) ExpectEqual(!!(cond), 0)

// Indicates that the function does not return (i.e. aborts or raises an exception).
#if COMPILER_HAS_CPP_ATTRIB(noreturn)
	#define NORETURN [[noreturn]]
#elif COMPILER_HAS_GCC_ATTRIB(noreturn)
	#define NORETURN __attribute__((noreturn))
#else
	#define NORETURN
#endif

// Indicates that the function's return value should not be discarded.
#if COMPILER_HAS_CPP_ATTRIB(nodiscard)
	#define NODISCARD [[nodiscard]]
#elif COMPILER_HAS_GCC_ATTRIB(nodiscard)
	#define NODISCARD __attribute__((nodiscard))
#else
	#define NODISCARD
#endif

// Indicates that the compiler should ignore its inlining heuristics and always attempt to inline
// the function. Note that MSVC does not obey this if compiling with /Ob0.
#if defined(_MSC_VER)
	#define FORCEINLINE __forceinline
#elif COMPILER_HAS_GCC_ATTRIB(always_inline)
	#define FORCEINLINE inline __attribute__((always_inline))
#else
	#define FORCEINLINE inline
#endif

// Turns a macro expansion result into a string.
#define STRINGIZE1(x) STRINGIZE0(x)
#define STRINGIZE0(x) #x

// Produces a compilation error if the given condition is false.
#if !((__cplusplus >= 200410L) || (_MSC_VER >= 1800) || defined(static_assert))
	#define static_assert(cond, msg) ((void)sizeof(char[1 - 2*!(condition)]))
#endif
#define StaticAssert(cond) static_assert(cond, STRINGIZE1(cond))

// Macros for making code conditional on the build configuration.
#if defined(NDEBUG)
	#define DEBUG 0
	#define DEBUG_ONLY(...)
#else
	#define DEBUG 1
	#define DEBUG_ONLY(...) __VA_ARGS__
#endif

// Invokes undefined behaviour if reached.
#if COMPILER_HAS_BUILTIN(__builtin_unreachable)
	#define Unreachable() __builtin_unreachable()
#elif COMPILER_IS_MSVC(0)
	#define Unreachable() __assume(0)
#else
	#define Unreachable() abort()
#endif

/***************************************************************************************************
 * Platform detection macros. References:
 * https://github.com/electronicarts/EABase/blob/master/include/Common/EABase/config/eaplatform.h
 * https://sourceforge.net/p/predef/wiki/OperatingSystems/
 * https://gist.github.com/jtbandes/cd8ee0cf139ae4411b41 (on TargetConditionals.h)
 **************************************************************************************************/

#define PLATFORM_BIT_WEB     ((1 <<  0))
#define PLATFORM_BIT_DESKTOP ((1 <<  1))
#define PLATFORM_BIT_MOBILE  ((1 <<  2))
#define PLATFORM_BIT_UNIX    ((1 <<  3))
#define PLATFORM_BIT_APPLE   ((1 <<  4) | PLATFORM_BIT_UNIX)
#define PLATFORM_BIT_LINUX   ((1 <<  5) | PLATFORM_BIT_UNIX)
#define PLATFORM_BIT_WINDOWS ((1 <<  6) | PLATFORM_BIT_DESKTOP)
#define PLATFORM_BIT_ANDROID ((1 <<  7) | PLATFORM_BIT_LINUX | PLATFORM_BIT_MOBILE)
#define PLATFORM_BIT_MACOS   ((1 <<  8) | PLATFORM_BIT_APPLE | PLATFORM_BIT_DESKTOP)
#define PLATFORM_BIT_IOS     ((1 <<  9) | PLATFORM_BIT_APPLE | PLATFORM_BIT_MOBILE)
#define PLATFORM_BIT_TVOS    ((1 << 10) | PLATFORM_BIT_APPLE | PLATFORM_BIT_MOBILE)

#define PLATFORM_WEB     (PLATFORM_BITS & PLATFORM_BIT_WEB)
#define PLATFORM_DESKTOP (PLATFORM_BITS & PLATFORM_BIT_DESKTOP)
#define PLATFORM_MOBILE  (PLATFORM_BITS & PLATFORM_BIT_MOBILE)
#define PLATFORM_UNIX    (PLATFORM_BITS & PLATFORM_BIT_UNIX)
#define PLATFORM_APPLE   (PLATFORM_BITS & PLATFORM_BIT_APPLE)
#define PLATFORM_LINUX   (PLATFORM_BITS & PLATFORM_BIT_LINUX)
#define PLATFORM_WINDOWS (PLATFORM_BITS & PLATFORM_BIT_WINDOWS)
#define PLATFORM_ANDROID (PLATFORM_BITS & PLATFORM_BIT_ANDROID)
#define PLATFORM_MACOS   (PLATFORM_BITS & PLATFORM_BIT_MACOS)
#define PLATFORM_IOS     (PLATFORM_BITS & PLATFORM_BIT_IOS)
#define PLATFORM_TVOS    (PLATFORM_BITS & PLATFORM_BIT_TVOS)

#if defined(EMSCRIPTEN)
	#define PLATFORM_BITS PLATFORM_BIT_WEB
	#define PLATFORM_NAME "Emscripten"
#elif defined(_WIN32)
	#define PLATFORM_BITS PLATFORM_BIT_WINDOWS
	#define PLATFORM_NAME "Windows"
#elif defined(__APPLE__)
	#include <TargetConditionals.h>
	#if defined(TARGET_OS_TV) && TARGET_OS_TV
		#define PLATFORM_BITS PLATFORM_BIT_TVOS
		#define PLATFORM_NAME "tvOS"
	#elif (defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (!defined(TARGET_OS_IOS) && TARGET_OS_IPHONE)
		#define PLATFORM_BITS PLATFORM_BIT_IOS
		#define PLATFORM_NAME "iOS"
	#else
		#define PLATFORM_BITS PLATFORM_BIT_MACOS
		#if TARGET_OS_MAC
			#define PLATFORM_NAME "macOS"
		#else
			#define PLATFORM_NAME "Apple"
			#pragma message(__FILE__ ": Unknown platform, assuming macOS")
		#endif
	#endif
#elif defined(__ANDROID__)
	#define PLATFORM_BITS PLATFORM_BIT_ANDROID
	#define PLATFORM_NAME "Android"
#elif defined(__linux__)
	#define PLATFORM_BITS PLATFORM_BIT_LINUX
	#define PLATFORM_NAME "Linux"
#else
	#define PLATFORM_BITS PLATFORM_BIT_UNIX
	#define PLATFORM_NAME "POSIX"
	#pragma message(__FILE__ ": Unknown platform, assuming POSIX-compatible")
#endif

/***************************************************************************************************
 * Generally useful utility functions
 **************************************************************************************************/

// Returns the size in elements of an array, when statically known.
template <typename R = uint32_t, typename T, size_t N>
constexpr R CountOf(T const (&)[N]) noexcept { return R(N); }

// Returns the smallest of its two arguments.
template <typename T, typename U>
static FORCEINLINE constexpr T Min(const T x, const U y) { return (x < y) ? x : T(y); }

// Returns the largest of its two arguments.
template <typename T, typename U>
static FORCEINLINE constexpr T Max(const T x, const U y) { return (x >= y) ? x : T(y); }

// Clamps a value to a given range.
template <typename T, typename U, typename V>
static FORCEINLINE constexpr T Clamp(const T x, const U min, const V max) {
	T tmin = T(min), tmax = T(max);
	return (x < tmin) ? tmin : (x > tmax) ? tmax : x;
}
