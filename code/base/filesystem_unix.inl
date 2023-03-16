#include "base/filesystem.hh"
#include "base/debug.hh"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

String GetCurrentDir() {
	// FIXME: getcwd(NULL, 0) is an extension, probably doesn't work on Android
	// FIXME: Check for and handle errors
	char* buf = getcwd(NULL, 0);
	return String::move(buf);
}

bool SetCurrentDir(const String& path) {
	return (chdir(path) == 0);
}

bool PathExists(const String& path) {
	struct stat statbuf;
	if (stat(path, &statbuf) != 0) { return false; }
	return true;
}

bool PathIsFile(const String& path) {
	struct stat statbuf;
	if (stat(path, &statbuf) != 0) { return false; }
	return S_ISREG(statbuf.st_mode);
}

bool PathIsDirectory(const String& path) {
	struct stat statbuf;
	if (stat(path, &statbuf) != 0) { return false; }
	return S_ISDIR(statbuf.st_mode);
}

uint64_t GetFileModificationTime(const String& path) {
	struct stat statbuf;
	if (stat(path, &statbuf) != 0) { return false; }
	return static_cast<uint64_t>(statbuf.st_mtime);
}

// Platform-specific data for DirectoryIterator.
struct UnixDirectoryIterator {
	DIR* dfd;
	struct dirent* dp;
};

DirectoryIterator& DirectoryIterator::begin() {
	auto& data = *(reinterpret_cast<UnixDirectoryIterator*>(platform_data));

	data.dfd = opendir(root);
	if (!data.dfd) {
		current = nullptr;
		return *this;
	}

	data.dp = readdir(data.dfd);
	current = data.dp ? String::view(data.dp->d_name) : nullptr;

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
	auto& data = *(reinterpret_cast<UnixDirectoryIterator*>(platform_data));

	data.dp = readdir(data.dfd);
	current = data.dp ? String::view(data.dp->d_name) : nullptr;
	if (!data.dp) {
		closedir(data.dfd);
	}

	return *this;
}
