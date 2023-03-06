// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
