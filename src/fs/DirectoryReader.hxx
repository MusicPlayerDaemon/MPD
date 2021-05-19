/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "Path.hxx"

#ifdef _WIN32

#include <fileapi.h>
#include <tchar.h>

/**
 * Reader for directory entries.
 */
class DirectoryReader {
	const HANDLE handle;
	WIN32_FIND_DATA data;
	bool first = true;

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
	 *
	 * Throws std::system_error on error.
	 */
	explicit DirectoryReader(Path dir);

	DirectoryReader(const DirectoryReader &other) = delete;
	DirectoryReader &operator=(const DirectoryReader &other) = delete;

	/**
	 * Destroys this instance.
	 */
	~DirectoryReader() {
		FindClose(handle);
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
	dirent *ent = nullptr;

public:
	/**
	 * Creates new directory reader for the specified #dir.
	 *
	 * Throws std::system_error on error.
	 */
	explicit DirectoryReader(Path dir);

	DirectoryReader(const DirectoryReader &other) = delete;
	DirectoryReader &operator=(const DirectoryReader &other) = delete;

	/**
	 * Destroys this instance.
	 */
	~DirectoryReader() {
		closedir(dirp);
	}

	/**
	 * Checks if directory entry is available.
	 */
	bool HasEntry() const {
		return ent != nullptr;
	}

	/**
	 * Reads next directory entry.
	 */
	bool ReadEntry() {
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
