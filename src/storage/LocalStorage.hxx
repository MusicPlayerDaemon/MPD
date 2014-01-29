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

#ifndef MPD_STORAGE_LOCAL_HXX
#define MPD_STORAGE_LOCAL_HXX

#include "check.h"
#include "fs/AllocatedPath.hxx"
#include "fs/DirectoryReader.hxx"

#include <string>

struct FileInfo;

class LocalDirectoryReader {
	AllocatedPath base_fs;

	DirectoryReader reader;

	std::string name_utf8;

public:
	LocalDirectoryReader(AllocatedPath &&_base_fs)
		:base_fs(std::move(_base_fs)), reader(base_fs) {}

	bool HasFailed() {
		return reader.HasFailed();
	}

	const char *Read();

	bool GetInfo(bool follow, FileInfo &info, Error &error);
};

class LocalStorage {
	const std::string base_utf8;
	const AllocatedPath base_fs;

public:
	LocalStorage(const char *_base_utf8, Path _base_fs)
		:base_utf8(_base_utf8), base_fs(_base_fs) {}

	LocalStorage(const LocalStorage &) = delete;

	bool GetInfo(const char *uri_utf8, bool follow, FileInfo &info,
		     Error &error);

	LocalDirectoryReader *OpenDirectory(const char *uri_utf8,
					    Error &error);

	/**
	 * Map the given relative URI to an absolute URI.
	 */
	gcc_pure
	std::string MapUTF8(const char *uri_utf8) const;

	/**
	 * Map the given relative URI to a local file path.  Returns
	 * AllocatedPath::Null() on error or if this storage does not
	 * support local files.
	 */
	gcc_pure
	AllocatedPath MapFS(const char *uri_utf8) const;

	gcc_pure
	AllocatedPath MapChildFS(const char *uri_utf8,
				 const char *child_utf8) const;

private:
	AllocatedPath MapFS(const char *uri_utf8, Error &error) const;
};

#endif
