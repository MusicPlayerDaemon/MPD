// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FS_FILESYSTEM_HXX
#define MPD_FS_FILESYSTEM_HXX

#include "Path.hxx"
#include "io/UniqueFileDescriptor.hxx"

#ifdef _WIN32
#include <fileapi.h>
#endif

#include <sys/stat.h>
#include <unistd.h>

class AllocatedPath;

/**
 * Wrapper for open_cloexec() that uses #Path names.
 */
static inline UniqueFileDescriptor
OpenFile(Path file, int flags, int mode)
{
	UniqueFileDescriptor fd;
	fd.Open(file.c_str(), flags, mode);
	return fd;
}

/*
 * Wrapper for rename() that uses #Path names.
 *
 * Throws std::system_error on error.
 */
void
RenameFile(Path oldpath, Path newpath);

#ifndef _WIN32

/**
 * Wrapper for stat() that uses #Path names.
 */
static inline bool
StatFile(Path file, struct stat &buf, bool follow_symlinks = true)
{
	int ret = follow_symlinks
		? stat(file.c_str(), &buf)
		: lstat(file.c_str(), &buf);
	return ret == 0;
}

#endif

static inline bool
CreateDirectoryNoThrow(Path path) noexcept
{
#ifdef _WIN32
	return CreateDirectory(path.c_str(), nullptr);
#else
	return mkdir(path.c_str(), 0777);
#endif
}

/**
 * Truncate a file that exists already.  Throws std::system_error on
 * error.
 */
void
TruncateFile(Path path);

/**
 * Wrapper for unlink() that uses #Path names.  Throws
 * std::system_error on error.
 */
void
RemoveFile(Path path);

/**
 * Wrapper for readlink() that uses #Path names.
 */
AllocatedPath
ReadLink(Path path) noexcept;

#ifndef _WIN32

static inline bool
MakeFifo(Path path, mode_t mode) noexcept
{
	return mkfifo(path.c_str(), mode) == 0;
}

/**
 * Wrapper for access() that uses #Path names.
 */
[[gnu::pure]]
static inline bool
CheckAccess(Path path, int mode) noexcept
{
	return access(path.c_str(), mode) == 0;
}

#endif

/**
 * Checks if #Path exists and is a regular file.
 */
[[gnu::pure]]
static inline bool
FileExists(Path path, bool follow_symlinks = true) noexcept
{
#ifdef _WIN32
	(void)follow_symlinks;

	const auto a = GetFileAttributes(path.c_str());
	return a != INVALID_FILE_ATTRIBUTES &&
		(a & (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_DEVICE)) == 0;
#else
	struct stat buf;
	return StatFile(path, buf, follow_symlinks) && S_ISREG(buf.st_mode);
#endif
}

/**
 * Checks if #Path exists and is a directory.
 */
[[gnu::pure]]
static inline bool
DirectoryExists(Path path, bool follow_symlinks = true) noexcept
{
#ifdef _WIN32
	(void)follow_symlinks;

	const auto a = GetFileAttributes(path.c_str());
	return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
	struct stat buf;
	return StatFile(path, buf, follow_symlinks) && S_ISDIR(buf.st_mode);
#endif
}

/**
 * Checks if #Path exists.
 */
[[gnu::pure]]
static inline bool
PathExists(Path path) noexcept
{
#ifdef _WIN32
	return GetFileAttributes(path.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
	return CheckAccess(path, F_OK);
#endif
}

#endif
