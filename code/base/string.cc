#include "base/string.hh"

#include <stdlib.h>
#include <string.h>
#include <stb_sprintf.h>

String::String(uint32_t size): cstr{nullptr}, capacity{size ? size + 1 : 0}, _size{0} {
	if (ExpectTrue(size != 0)) {
		const_cast<char*&>(cstr) = static_cast<char*>(calloc(capacity, 1));
	}
}

String String::copy(const char* cstr, uint32_t size) {
	if (ExpectFalse(!cstr)) { return String(); }
	char* new_cstr = static_cast<char*>(malloc(size + 1));
	memcpy(new_cstr, cstr, size);
	new_cstr[size] = '\0';
	return String(new_cstr, size + 1, static_cast<uint32_t>(strlen(new_cstr)));
}

String String::copy(const char* cstr) {
	if (ExpectFalse(!cstr)) { return String(); }
	uint32_t len = static_cast<uint32_t>(strlen(cstr));
	return String::copy(cstr, len);
}

String String::move(const char* cstr) {
	uint32_t size = static_cast<uint32_t>(strlen(cstr));
	return String(cstr, size + 1, size);
}

String String::join(String a, String b) {
	uint32_t asize = a.size(), bsize = b.size(), size = asize + bsize;
	char* new_cstr = static_cast<char*>(malloc(size + 1));
	memcpy(&new_cstr[0], a.cstr, asize);
	memcpy(&new_cstr[asize], b.cstr, bsize);
	new_cstr[size] = '\0';
	return String(new_cstr, size + 1, size);
}

String String::vformat(const char* fmt, va_list ap) {
	// snprintf returns the number of characters that would have been written if the buffer was
	// large enough. This count doesn't include the null terminator.
	uint32_t size = stbsp_vsnprintf(nullptr, 0, fmt, ap);
	char* buffer = static_cast<char*>(malloc(size + 1));
	stbsp_vsnprintf(buffer, size + 1, fmt, ap);
	return String(buffer, size + 1, size);
}

String String::format(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	String str = String::vformat(fmt, ap);
	va_end(ap);
	return str;
}
