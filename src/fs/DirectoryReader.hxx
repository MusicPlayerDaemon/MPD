/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

#ifndef MPD_FS_DIRECTORY_READER_HXX
#define MPD_FS_DIRECTORY_READER_HXX

#include "check.h"
#include "Path.hxx"

#ifdef WIN32

#include <windows.h>
#include <tchar.h>

/**
 * Reader for directory entries.
 */
class DirectoryReader {
	const HANDLE handle;
	WIN32_FIND_DATA data;
	bool first;

	class MakeWildcardPath {
		PathTraitsFS::pointer path;

	public:
		MakeWildcardPath(PathTraitsFS::const_pointer _path) {
			auto l = _tcslen(_path);
			path = new PathTraitsFS::value_type[l + 3];
			_tcscpy(path, _path);
			path[l] = _T('\\');
			path[l + 1] = _T('*');
			path[l + 2] = 0;
		}

		~MakeWildcardPath() {
			delete[] path;
		}

		operator PathTraitsFS::const_pointer() const {
			return path;
		}
	};

public:
	/**
	 * Creates new directory reader for the specified #dir.
	 */
	explicit DirectoryReader(Path dir)
		:handle(FindFirstFile(MakeWildcardPath(dir.c_str()), &data)),
		 first(true) {}

	DirectoryReader(const DirectoryReader &other) = delete;
	DirectoryReader &operator=(const DirectoryReader &other) = delete;

	/**
	 * Destroys this instance.
	 */
	~DirectoryReader() {
		if (!HasFailed())
			FindClose(handle);
	}

	/**
	 * Checks if directory failed to open.
	 */
	bool HasFailed() const {
		return handle == INVALID_HANDLE_VALUE;
	}

	/**
	 * Reads next directory entry.
	 */
	bool ReadEntry() {
		if (first) {
			first = false;
			return true;
		}

		return FindNextFile(handle, &data) != 0;
	}

	/**
	 * Extracts directory entry that was previously read by #ReadEntry.
	 */
	Path GetEntry() const {
		return Path::FromFS(data.cFileName);
	}
};

#else

#include <dirent.h>

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
	explicit DirectoryReader(Path dir)
		: dirp(opendir(dir.c_str())),
		  ent(nullptr) {
	}

	DirectoryReader(const DirectoryReader &other) = delete;
	DirectoryReader &operator=(const DirectoryReader &other) = delete;

	/**
	 * Destroys this instance.
	 */
	~DirectoryReader() {
		if (!HasFailed())
			closedir(dirp);
	}

	/**
	 * Checks if directory failed to open. 
	 */
	bool HasFailed() const {
		return dirp == nullptr;
	}

	/**
	 * Checks if directory entry is available.
	 */
	bool HasEntry() const {
		assert(!HasFailed());
		return ent != nullptr;
	}

	/**
	 * Reads next directory entry.
	 */
	bool ReadEntry() {
		assert(!HasFailed());
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

#endif
