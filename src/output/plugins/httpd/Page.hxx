/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#ifndef MPD_PAGE_HXX
#define MPD_PAGE_HXX

#include "util/AllocatedArray.hxx"

#include <memory>

#include <stddef.h>
#include <stdint.h>

/**
 * A dynamically allocated buffer which keeps track of its reference
 * count.  This is useful for passing buffers around, when several
 * instances hold references to one buffer.
 */
class Page {
	AllocatedArray<uint8_t> buffer;

public:
	explicit Page(size_t _size):buffer(_size) {}
	explicit Page(AllocatedArray<uint8_t> &&_buffer)
		:buffer(std::move(_buffer)) {}

	Page(const void *data, size_t size);

	size_t GetSize() const {
		return buffer.size();
	}

	const uint8_t *GetData() const {
		return &buffer.front();
	}
};

typedef std::shared_ptr<Page> PagePtr;

#endif
