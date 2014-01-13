/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_FS_FILESYSTEM_HXX
#define MPD_FS_FILESYSTEM_HXX

#include "check.h"
#include "Traits.hxx"
#include "system/fd_util.h"

#include "Path.hxx"

#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

class AllocatedPath;

namespace FOpenMode {
	/**
	 * Open mode for reading text files.
	 */
	constexpr PathTraitsFS::const_pointer ReadText = "r";

	/**
	 * Open mode for reading binary files.
	 */
	constexpr PathTraitsFS::const_pointer ReadBinary = "rb";

	/**
	 * Open mode for writing text files.
	 */
	constexpr PathTraitsFS::const_pointer WriteText = "w";

	/**
	 * Open mode for writing binary files.
	 */
	constexpr PathTraitsFS::const_pointer WriteBinary = "wb";

	/**
	 * Open mode for appending text files.
	 */
	constexpr PathTraitsFS::const_pointer AppendText = "a";

	/**
	 * Open mode for appending binary files.
	 */
	constexpr PathTraitsFS::const_pointer AppendBinary = "ab";
}

/**
 * Wrapper for fopen() that uses #Path names.
 */
static inline FILE *
FOpen(Path file, PathTraitsFS::const_pointer mode)
{
	return fopen(file.c_str(), mode);
}

/**
 * Wrapper for open_cloexec() that uses #Path names.
 */
static inline int
OpenFile(Path file, int flags, int mode)
{
	return open_cloexec(file.c_str(), flags, mode);
}

/**
 * Wrapper for rename() that uses #Path names.
 */
static inline bool
RenameFile(Path oldpath, Path newpath)
{
	return rename(oldpath.c_str(), newpath.c_str()) == 0;
}

/**
 * Wrapper for stat() that uses #Path names.
 */
static inline bool
StatFile(Path file, struct stat &buf, bool follow_symlinks = true)
{
#ifdef WIN32
	(void)follow_symlinks;
	return stat(file.c_str(), &buf) == 0;
#else
	int ret = follow_symlinks
		? stat(file.c_str(), &buf)
		: lstat(file.c_str(), &buf);
	return ret == 0;
#endif
}

/**
 * Wrapper for unlink() that uses #Path names.
 */
static inline bool
RemoveFile(Path file)
{
	return unlink(file.c_str()) == 0;
}

/**
 * Wrapper for readlink() that uses #Path names.
 */
AllocatedPath
ReadLink(Path path);

#ifndef WIN32

static inline bool
MakeFifo(Path path, mode_t mode)
{
	return mkfifo(path.c_str(), mode) == 0;
}

/**
 * Wrapper for access() that uses #Path names.
 */
static inline bool
CheckAccess(Path path, int mode)
{
	return access(path.c_str(), mode) == 0;
}

#endif

/**
 * Checks is specified path exists and accessible.
 */
static inline bool
CheckAccess(Path path)
{
#ifdef WIN32
	struct stat buf;
	return StatFile(path, buf);
#else
	return CheckAccess(path, F_OK);
#endif
}

/**
 * Checks if #Path exists and is a regular file.
 */
static inline bool
FileExists(Path path, bool follow_symlinks = true)
{
	struct stat buf;
	return StatFile(path, buf, follow_symlinks) && S_ISREG(buf.st_mode);
}

/**
 * Checks if #Path exists and is a directory.
 */
static inline bool
DirectoryExists(Path path, bool follow_symlinks = true)
{
	struct stat buf;
	return StatFile(path, buf, follow_symlinks) && S_ISDIR(buf.st_mode);
}

/**
 * Checks if #Path exists.
 */
static inline bool
PathExists(Path path, bool follow_symlinks = true)
{
	struct stat buf;
	return StatFile(path, buf, follow_symlinks);
}

#endif
