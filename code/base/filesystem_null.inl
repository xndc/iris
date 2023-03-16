#include "base/filesystem.hh"
#include "base/debug.hh"

String GetCurrentDir() {
	LOG_F(WARNING, "%s is not supported on " PLATFORM_NAME, __func__);
	return String();
}

bool SetCurrentDir(const String& path) {
	LOG_F(WARNING, "%s is not supported on " PLATFORM_NAME, __func__);
	return false;
}

bool PathExists(const String& path) {
	LOG_F(WARNING, "%s is not supported on " PLATFORM_NAME, __func__);
	return false;
}

bool PathIsFile(const String& path) {
	LOG_F(WARNING, "%s is not supported on " PLATFORM_NAME, __func__);
	return false;
}

bool PathIsDirectory(const String& path) {
	LOG_F(WARNING, "%s is not supported on " PLATFORM_NAME, __func__);
	return false;
}

uint64_t GetFileModificationTime(const String& path) {
	LOG_F(WARNING, "%s is not supported on " PLATFORM_NAME, __func__);
	return 0;
}

DirectoryIterator& DirectoryIterator::begin() {
	LOG_F(WARNING, "DirectoryIterator is not supported on " PLATFORM_NAME);
	return *this;
}

DirectoryIterator& DirectoryIterator::operator++() {
	return *this;
}
