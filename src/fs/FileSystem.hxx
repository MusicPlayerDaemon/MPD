/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "fd_util.h"

#include "Path.hxx"

#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>

namespace FOpenMode {
/**
 * Open mode for reading text files.
 */
constexpr Path::const_pointer ReadText = "r";

/**
 * Open mode for reading binary files.
 */
constexpr Path::const_pointer ReadBinary = "rb";

/**
 * Open mode for writing text files.
 */
constexpr Path::const_pointer WriteText = "w";

/**
 * Open mode for writing binary files.
 */
constexpr Path::const_pointer WriteBinary = "wb";
}

/**
 * Wrapper for fopen() that uses #Path names.
 */
static inline FILE *FOpen(const Path &file, Path::const_pointer mode)
{
	return fopen(file.c_str(), mode);
}

/**
 * Wrapper for open_cloexec() that uses #Path names.
 */
static inline int OpenFile(const Path &file, int flags, int mode)
{
	return open_cloexec(file.c_str(), flags, mode);
}

/**
 * Wrapper for rename() that uses #Path names.
 */
static inline bool RenameFile(const Path &oldpath, const Path &newpath)
{
	return rename(oldpath.c_str(), newpath.c_str()) == 0;
}

/**
 * Wrapper for stat() that uses #Path names.
 */
static inline bool StatFile(const Path &file, struct stat &buf,
			    bool follow_symlinks = true)
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
static inline bool RemoveFile(const Path &file)
{
	return unlink(file.c_str()) == 0;
}

/**
 * Wrapper for readlink() that uses #Path names.
 */
Path ReadLink(const Path &path);

/**
 * Wrapper for access() that uses #Path names.
 */
static inline bool CheckAccess(const Path &path, int mode)
{
#ifdef WIN32
	(void)path;
	(void)mode;
	return true;
#else
	return access(path.c_str(), mode) == 0;
#endif
}

/**
 * Checks if #Path exists and is a regular file.
 */
static inline bool FileExists(const Path &path,
			      bool follow_symlinks = true)
{
	struct stat buf;
	return StatFile(path, buf, follow_symlinks) && S_ISREG(buf.st_mode);
}

/**
 * Checks if #Path exists and is a directory.
 */
static inline bool DirectoryExists(const Path &path,
				   bool follow_symlinks = true)
{
	struct stat buf;
	return StatFile(path, buf, follow_symlinks) && S_ISDIR(buf.st_mode);
}

/**
 * Checks if #Path exists.
 */
static inline bool PathExists(const Path &path,
			      bool follow_symlinks = true)
{
	struct stat buf;
	return StatFile(path, buf, follow_symlinks);
}

#endif
