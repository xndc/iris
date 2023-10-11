#include "base/filesystem.hh"
#include "base/debug.hh"

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <Windows.h>

#include <mutex>

// GetCurrentDirectory, SetCurrentDirectory and GetFullPathName are not thread-safe
static std::mutex Win32RelativePathMutex;

String GetCurrentDir() {
	std::lock_guard<std::mutex> guard(Win32RelativePathMutex);
	DWORD len_incl_nul = GetCurrentDirectoryA(0, NULL);
	auto str = String(len_incl_nul - 1);
	// FIXME: Win32: should use UTF-16 functions and do UTF-8 conversion internally
	// FIXME: Handle failure (check for zero and call GetLastError)
	GetCurrentDirectoryA(len_incl_nul, str.mut());
	return str;
}

bool SetCurrentDir(const String& path) {
	// We can't rely on SetCurrentDirectoryA's return value. Calling it with ".." from the root of a
	// drive will return true even though the directory doesn't change.
	String before = GetCurrentDir();
	// FIXME: Win32: should use UTF-16 functions and do UTF-8 conversion internally
	Win32RelativePathMutex.lock();
	BOOL ok = SetCurrentDirectoryA(path);
	Win32RelativePathMutex.unlock();
	if (!ok) { return false; }
	String after = GetCurrentDir();
	return (before != after);
}

bool PathExists(const String& path) {
	WIN32_FIND_DATAA find_data;
	HANDLE find_handle = FindFirstFileA(path, &find_data);
	if (find_handle != INVALID_HANDLE_VALUE) {
		FindClose(find_handle);
		return true;
	}
	return false;
}

bool PathIsFile(const String& path) {
	WIN32_FIND_DATAA find_data;
	HANDLE find_handle = FindFirstFileA(path, &find_data);
	if (find_handle != INVALID_HANDLE_VALUE) {
		// FIXME: Double-check logic here, the Win32 docs are not 100% clear
		// FIXME: Will this handle NTFS symlinks and junctions correctly?
		bool is_dir = !!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		bool is_normal = !!(find_data.dwFileAttributes & FILE_ATTRIBUTE_NORMAL);
		bool is_special = !!(find_data.dwFileAttributes & (FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_VIRTUAL));
		bool is_file = (is_normal || (!is_dir && !is_special));
		FindClose(find_handle);
		return is_file;
	}
	return false;
}

bool PathIsDirectory(const String& path) {
	WIN32_FIND_DATAA find_data;
	HANDLE find_handle = FindFirstFileA(path, &find_data);
	if (find_handle != INVALID_HANDLE_VALUE) {
		// FIXME: Will this handle NTFS symlinks and junctions correctly?
		bool is_dir = !!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		FindClose(find_handle);
		return is_dir;
	}
	return false;
}

uint64_t GetFileModificationTime(const String& path) {
	WIN32_FIND_DATAA find_data;
	HANDLE find_handle = FindFirstFileA(path, &find_data);
	if (find_handle != INVALID_HANDLE_VALUE) {
		FILETIME ft = find_data.ftLastWriteTime;
		ULARGE_INTEGER fti = {{ft.dwLowDateTime, ft.dwHighDateTime}};
		return static_cast<uint64_t>(fti.QuadPart);
	}
	return 0;
}

// Platform-specific data for DirectoryIterator.
struct Win32DirectoryIterator {
	WIN32_FIND_DATAA find_data;
	HANDLE find_handle;
	String pattern;
};
static_assert(sizeof(Win32DirectoryIterator) <= sizeof(DirectoryIterator::platform_data), "");

DirectoryIterator& DirectoryIterator::begin() {
	auto& data = *(reinterpret_cast<Win32DirectoryIterator*>(platform_data));

	if (ExpectFalse(data.find_handle)) {
		FindClose(data.find_handle);
	}

	// Windows wants this \* pattern to list files inside a directory
	data.pattern = PathJoin(root, "\\*");

	data.find_handle = FindFirstFileA(data.pattern, &data.find_data);
	if (data.find_handle == INVALID_HANDLE_VALUE) {
		current = nullptr;
		return *this;
	}
	current = data.find_data.cFileName;

	// Skip over the . and .. directories
	while (current) {
		bool is_dot1 = (current.size() == 1 && current[0] == '.');
		bool is_dot2 = (current.size() == 2 && current[0] == '.' && current[1] == '.');
		if (is_dot1 || is_dot2) {
			// Call DirectoryIterator::operator++ to move to next directory (changes `current`)
			++(*this);
		} else {
			break;
		}
	}

	return *this;
}

DirectoryIterator& DirectoryIterator::operator++() {
	auto& data = *(reinterpret_cast<Win32DirectoryIterator*>(platform_data));

	if (current != nullptr) {
		if (FindNextFileA(data.find_handle, &data.find_data)) {
			current = data.find_data.cFileName;
		} else {
			current = nullptr;
		}
	}

	return *this;
}
