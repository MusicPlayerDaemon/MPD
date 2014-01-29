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

#ifndef MPD_STORAGE_FILE_INFO_HXX
#define MPD_STORAGE_FILE_INFO_HXX

#include "check.h"

#include <time.h>
#include <stdint.h>

struct FileInfo {
	enum class Type : uint8_t {
		OTHER,
		REGULAR,
		DIRECTORY,
	};

	Type type;

	/**
	 * The file size in bytes.  Only valid for #Type::REGULAR.
	 */
	uint64_t size;

	/**
	 * The modification time.  0 means unknown / not applicable.
	 */
	time_t mtime;

	/**
	 * Device id and inode number.  0 means unknown / not
	 * applicable.
	 */
	unsigned device, inode;

	bool IsRegular() const {
		return type == Type::REGULAR;
	}

	bool IsDirectory() const {
		return type == Type::DIRECTORY;
	}
};

#endif
