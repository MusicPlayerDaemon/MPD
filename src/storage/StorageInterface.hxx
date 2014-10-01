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

#ifndef MPD_STORAGE_INTERFACE_HXX
#define MPD_STORAGE_INTERFACE_HXX

#include "check.h"
#include "Compiler.h"

#include <string>

struct FileInfo;
class AllocatedPath;
class Error;

class StorageDirectoryReader {
public:
	StorageDirectoryReader() = default;
	StorageDirectoryReader(const StorageDirectoryReader &) = delete;
	virtual ~StorageDirectoryReader() {}

	virtual const char *Read() = 0;
	virtual bool GetInfo(bool follow, FileInfo &info, Error &error) = 0;
};

class Storage {
public:
	Storage() = default;
	Storage(const Storage &) = delete;
	virtual ~Storage() {}

	virtual bool GetInfo(const char *uri_utf8, bool follow, FileInfo &info,
			     Error &error) = 0;

	virtual StorageDirectoryReader *OpenDirectory(const char *uri_utf8,
						      Error &error) = 0;

	/**
	 * Map the given relative URI to an absolute URI.
	 */
	gcc_pure
	virtual std::string MapUTF8(const char *uri_utf8) const = 0;

	/**
	 * Map the given relative URI to a local file path.  Returns
	 * AllocatedPath::Null() on error or if this storage does not
	 * support local files.
	 */
	gcc_pure
	virtual AllocatedPath MapFS(const char *uri_utf8) const;

	gcc_pure
	AllocatedPath MapChildFS(const char *uri_utf8,
				 const char *child_utf8) const;

	/**
	 * Check if the given URI points inside this storage.  If yes,
	 * then it returns a relative URI (pointing inside the given
	 * string); if not, returns nullptr.
	 */
	gcc_pure
	virtual const char *MapToRelativeUTF8(const char *uri_utf8) const = 0;
};

#endif
