#pragma once

#include "base/base.hh"

#include <string.h>
#include <stdarg.h>

/* Hybrid byte-string type designed to accomodate multiple allocation scenarios.
 *
 * Guaranteed to be null-terminated, so it can be passed to APIs that expect C strings.
 * Can either manage its own memory or point to an arbitrary null-terminated buffer.
 * Lightweight, hopefully near-zero overhead for common cases.
 *
 * Use `String::view()` to create a view into another String or null-terminated buffer.
 * Use `String::copy()` to create a copy of a given String or null-terminated buffer.
 * Use `String::move()` to create a String that manages the given buffer's ownership.
 *
 * There's an implicit conversion from char* that acts like String::view(), so passing a char*
 * argument to a function that accepts a String will "just work" without any performance penalty.
 *
 * Strings are immutable by default. Use mut() to get a writable char* pointer.
 */
struct String {
	// Pointer to backing buffer. Either a null-terminated buffer or nullptr.
	const char* const cstr;
	// Capacity of backing buffer in bytes, including null terminator, if managed by this object.
	// A capacity of zero means this string is a view into an externally managed buffer.
	const uint32_t capacity;
	// Cached result of size(). Shouldn't be used directly.
	const uint32_t _size;

	// Default constructor. Returns a null string.
	FORCEINLINE constexpr String(): cstr{nullptr}, capacity{0}, _size{0} {}

	// Allocates an empty string with the given capacity, plus room for a null terminator.
	String(uint32_t size);

	// Explicit constructor that fills all fields out. Intended for internal use.
	FORCEINLINE constexpr String(const char* cstr, uint32_t capacity, uint32_t length):
		cstr{cstr}, capacity{capacity}, _size{0} {}

	// Implicit conversion from char*. Produces a String that acts as a view into another buffer.
	FORCEINLINE String(const char* cstr): String{cstr, 0, 0} {}

	// Returns a String that acts as a view into another buffer. The buffer must be null-terminated.
	// Care should be taken to make sure that this String doesn't outlive the buffer.
	FORCEINLINE static String view(const char* cstr) { return String(cstr); }

	// Returns a String that acts as a view into another String. Care should be taken to make sure
	// that this String doesn't outlive its target.
	FORCEINLINE static String view(const String& str) {
		return String(str.cstr, str.capacity, str._size);
	}

	// Copies a block of memory starting at the given char* pointer into a String.
	static String copy(const char* cstr, uint32_t size);

	// Copies the given buffer, which must be null-terminated, into a String.
	static String copy(const char* cstr);

	// Returns a copy of the given String.
	FORCEINLINE static String copy(const String& str) { return String::copy(str.cstr, str.size()); }

	// Copying constructor. Returns a copy of the given String.
	FORCEINLINE String(const String& str): String{String::copy(str.cstr, str.size())} {}

	// Copying assignment operator. Copies the right-hand-side String into the left-hand-side one,
	// making a copy of the underlying buffer.
	FORCEINLINE String& operator=(const String& rhs) { *this = String::copy(rhs); return *this; }

	// Wraps the given null-terminated buffer in a String that will manage its ownership. The buffer
	// must have been allocated with malloc() or similar. free() will be used to deallocate it.
	static String move(const char* cstr);

	// Move constructor. Invalidates the given String.
	FORCEINLINE constexpr String(String&& str): cstr{str.cstr}, capacity{str.capacity}, _size(str._size) {
		const_cast<const char*&>(str.cstr) = nullptr;
		const_cast<uint32_t&>(str.capacity) = 0;
		const_cast<uint32_t&>(str._size) = 0;
	}

	// Explicit move function, equivalent to the move constructor. Invalidates the given String.
	FORCEINLINE static String move(String&& str) { return String(std::move(str)); }

	// Moving assignment operator. Invalidates the right-hand-side String.
	FORCEINLINE constexpr String& operator=(String&& rhs) {
		const_cast<const char*&>(cstr) = rhs.cstr;
		const_cast<uint32_t&>(capacity) = rhs.capacity;
		const_cast<uint32_t&>(_size) = rhs._size;
		const_cast<const char*&>(rhs.cstr) = nullptr;
		const_cast<uint32_t&>(rhs.capacity) = 0;
		const_cast<uint32_t&>(rhs._size) = 0;
		return *this;
	}

	// Implicit conversion to const char*.
	FORCEINLINE constexpr operator const char*() const { return cstr; }

	// Implicit conversion to bool. Checks that the string is non-empty.
	FORCEINLINE constexpr operator bool() { return cstr && cstr[0] != '\0'; }

	// Equality test against null-terminated string or nullptr. Comparison performed with strcmp.
	FORCEINLINE bool operator==(const char* rhs) {
		return (cstr && rhs) ? (strcmp(cstr, rhs) == 0) : (cstr == rhs);
	}
	FORCEINLINE bool operator!=(const char* rhs) {
		return (cstr && rhs) ? (strcmp(cstr, rhs) != 0) : (cstr != rhs);
	}

	// Equality test against String. Comparison performed with strcmp. Null strings compare equal.
	FORCEINLINE bool operator==(const String rhs) {
		return (cstr && rhs.cstr) ? (strcmp(cstr, rhs.cstr) == 0) : (cstr == rhs.cstr);
	}
	FORCEINLINE bool operator!=(const String rhs) {
		return (cstr && rhs.cstr) ? (strcmp(cstr, rhs.cstr) != 0) : (cstr != rhs.cstr);
	}

	// Byte-level indexing operator. Does no bounds checking whatsoever.
	FORCEINLINE constexpr char operator[](size_t i) const { return cstr[i]; }

	// Retrieves this UString's length in bytes, not including any null terminators.
	FORCEINLINE uint32_t size() {
		if (_size == 0 && cstr) {
			const_cast<uint32_t&>(_size) = static_cast<uint32_t>(strlen(cstr));
		}
		return cstr ? _size : 0;
	}

	// Retrieves this UString's length in bytes, not including any null terminators.
	// Constant version that doesn't cache the computed value.
	FORCEINLINE uint32_t size() const {
		if (_size == 0 && cstr) {
			return static_cast<uint32_t>(strlen(cstr));
		}
		return cstr ? _size : 0;
	}

	// Methods for iterating over the string byte-by-byte, ending at the first null.
	// FIXME: Need to test these. I don't have the best track record with iterators.
	FORCEINLINE const String begin() const { return String(cstr, 0, 0); }
	FORCEINLINE const String end() const { return String(nullptr, 0, 0); }
	FORCEINLINE const String& operator++() {
		const_cast<const char*&>(cstr) = (cstr == nullptr || cstr[0] == '\0') ? nullptr : &cstr[1];
		return *this;
	}
	FORCEINLINE char& operator*() { return const_cast<char&>(cstr[0]); }

	// Retrieves a writable pointer into this String's backing buffer. If the String doesn't have
	// ownership of its buffer, this function will duplicate it into a new one.
	FORCEINLINE char* mut() {
		if (capacity == 0) { *this = String::copy(*this); }
		// Invalidate the size so it gets recomputed after the caller is done with this char*.
		// FIXME: This isn't exactly robust. Should we have an RAII wrapper for the mutable string?
		const_cast<uint32_t&>(_size) = 0;
		return const_cast<char*>(cstr);
	}

	// Joins two strings together, returning a newly allocated one.
	static String join(String a, String b);

	// Generates a string from a format string and arguments. Wrapper around stb_sprintf.
	static String format(const char* fmt, ...);
	static String vformat(const char* fmt, va_list ap);

	// Destructor. Deallocates the backing buffer if its lifetime is managed by this UString.
	FORCEINLINE ~String() {
		if (capacity != 0) { free((void*)(cstr)); }
	}
};
