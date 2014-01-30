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

#ifndef MPD_FS_DIRECTORY_READER_HXX
#define MPD_FS_DIRECTORY_READER_HXX

#include "check.h"
#include "Path.hxx"

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
