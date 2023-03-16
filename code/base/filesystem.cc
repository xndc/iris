#include "base/filesystem.hh"
#include "base/debug.hh"

String PathJoin(const String& a, const String& b) {
	const char native_sep  = PLATFORM_WINDOWS ? '\\' : '/';
	const char foreign_sep = PLATFORM_WINDOWS ? '/' : '\\';
	uint32_t asize = a.size(), bsize = b.size();
	// Do we need to add a separator between the strings?
	char a_end = a.size() ? a[a.size() - 1] : 0;
	char b_fst = b.size() ? b[0] : 0;
	bool a_end_is_sep = (a_end == '/' || a_end == '\\');
	bool b_fst_is_sep = (b_fst == '/' || b_fst == '\\');
	uint32_t sep_chars = (a_end_is_sep || b_fst_is_sep) ? 0 : 1;
	// Omit a's end separator if b starts with one
	if (a_end_is_sep && b_fst_is_sep) { asize -= 1; }
	// Copy a and b into a newly allocated String
	uint32_t size = asize + sep_chars + bsize;
	String path = String(size);
	memcpy(&path.mut()[0], a.cstr, asize);
	if (sep_chars) { path.mut()[asize] = native_sep; }
	memcpy(&path.mut()[asize + sep_chars], b.cstr, bsize);
	path.mut()[size] = '\0';
	// Rewrite string to get rid of mixed separators
	for (char& c : path) { if (c == foreign_sep) { c = native_sep; } }
	return path;
}

String ReadFile(const String& path) {
	FILE* file = fopen(path, "rb");
	if (!file) { return String(); }
	fseek(file, 0L, SEEK_END);
	auto str = String(static_cast<uint32_t>(ftell(file)));
	rewind(file);
	// FIXME: fread() can fail due to editor atomic autosave, if I remember correctly.
	size_t read = fread(str.mut(), sizeof(char), str.capacity - 1, file);
	str.mut()[read] = '\0';
	fclose(file);
	return str;
}

bool WriteFile(const String& path, const void* data, size_t size) {
	FILE* file = fopen(path, "wb");
	if (!file) { return false; }
	fwrite(data, 1, size, file);
	fclose(file);
	return true;
}

bool WriteFile(const String& path, const String& contents) {
	return WriteFile(path, contents.cstr, contents.size());
}

#if PLATFORM_WINDOWS
// This should be at the end of the file, to avoid polluting the rest with Windows.h stuff.
#include "base/filesystem_win.inl"
#else
#include "base/filesystem_unix.inl"
#endif
