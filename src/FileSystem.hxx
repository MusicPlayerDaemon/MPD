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

#ifndef MPD_FILESYSTEM_HXX
#define MPD_FILESYSTEM_HXX

#include "check.h"
#include "fd_util.h"

#include "Path.hxx"

#include <sys/stat.h>
#include <dirent.h>
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
static inline int RenameFile(const Path &oldpath, const Path &newpath)
{
	return rename(oldpath.c_str(), newpath.c_str());
}

/**
 * Wrapper for stat() that uses #Path names.
 */
static inline int StatFile(const Path &file, struct stat &buf)
{
	return stat(file.c_str(), &buf);
}

/**
 * Wrapper for unlink() that uses #Path names.
 */
static inline int UnlinkFile(const Path &file)
{
	return unlink(file.c_str());
}

/**
 * Wrapper for readlink() that uses #Path names.
 * Unlike readlink() it returns true on success and false otherwise.
 * Use errno to get error code.
 */
bool ReadLink(const Path &path, Path &result);

/**
 * Wrapper for access() that uses #Path names.
 */
static inline int CheckAccess(const Path &path, int mode)
{
#ifdef WIN32
	(void)path;
	(void)mode;
	return 0;
#else
	return access(path.c_str(), mode);
#endif
}

/**
 * Checks if #Path is a regular file.
 */
static inline bool CheckIsRegular(const Path &path)
{
	struct stat buf;
	return StatFile(path, buf) == 0 && S_ISREG(buf.st_mode);
}

/**
 * Checks if #Path is a directory.
 */
static inline bool CheckIsDirectory(const Path &path)
{
	struct stat buf;
	return StatFile(path, buf) == 0 && S_ISDIR(buf.st_mode);
}

/**
 * Checks if #Path exists.
 */
static inline bool CheckExists(const Path &path)
{
	struct stat buf;
	return StatFile(path, buf) == 0;
}

/**
 * Reader for directory entries.
 */
class DirectoryReader {
	DIR *const dirp;
	dirent *ent;
public:
	/**
	 * Creates new directory reader for the specified #dir.
	 */
	explicit DirectoryReader(const Path &dir)
		: dirp(opendir(dir.c_str())),
		  ent(nullptr) {
	}

	DirectoryReader(const DirectoryReader &other) = delete;
	DirectoryReader &operator=(const DirectoryReader &other) = delete;

	/**
	 * Destroys this instance.
	 */
	~DirectoryReader() {
		if (!Failed())
			closedir(dirp);
	}

	/**
	 * Checks if directory failed to open. 
	 */
	bool Failed() const {
		return dirp == nullptr;
	}

	/**
	 * Checks if directory entry is available.
	 */
	bool HasEntry() const {
		assert(!Failed());
		return ent != nullptr;
	}

	/**
	 * Reads next directory entry.
	 */
	bool ReadEntry() {
		assert(!Failed());
		ent = readdir(dirp);
		return HasEntry();
	}

	/**
	 * Extracts directory entry that was previously read by #ReadEntry.
	 */
	Path GetEntry() const {
		assert(HasEntry());
		return Path::FromFS(ent->d_name);
	}
};

#endif
