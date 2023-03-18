#pragma once
#include <stdio.h>
#include "base/string.hh"

// Join two paths together, adding a separator between them if needed.
String PathJoin(const String& a, const String& b);

// Get the current working directory.
String GetCurrentDir();
// Set the current working directory. Returns true if the directory was successfully changed.
bool SetCurrentDir(const String& path);

// Does the path point to a valid filesystem object?
bool PathExists(const String& path);
// Does the path point to a regular file?
bool PathIsFile(const String& path);
// Does the path point to a directory?
bool PathIsDirectory(const String& path);

// Get the specified file's last modification time.
uint64_t GetFileModificationTime(const String& path);

// Read a file from disk. Returns a String that will be deallocated when it goes out of scope.
// Assumes the file is binary data. Doesn't perform any newline conversion.
String ReadFile(const String& path);

// Write the given buffer to a file, overwriting its previous contents.
// Assumes the buffer is binary data and writes it to disk verbatim.
bool WriteFile(const String& path, const void* data, size_t size);

// Write the given string to a file, overwriting its previous contents.
// Assumes the buffer is binary data and writes it to disk verbatim, except the null terminator.
bool WriteFile(const String& path, const String& contents);

// Iterator that yields the names of entries inside a given directory.
struct DirectoryIterator {
	String root;
	String current;
	char platform_data[512];

	DirectoryIterator(const String& root) : root{String::copy(root)}, current{}, platform_data{} {}

	DirectoryIterator& begin();
	DirectoryIterator& operator++();
	String operator*() { return String::view(current); }

	const DirectoryIterator end() { return DirectoryIterator(root); }
	bool operator!=(const DirectoryIterator& rhs) { return current != rhs.current; }
};
