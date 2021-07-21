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

#include "PeekReader.hxx"

#include <algorithm>
#include <cassert>

#include <string.h>

const void *
PeekReader::Peek(size_t size)
{
	assert(size > 0);
	assert(size < sizeof(buffer));
	assert(buffer_size == 0);
	assert(buffer_position == 0);

	do {
		size_t nbytes = next.Read(buffer + buffer_size,
					  size - buffer_size);
		if (nbytes == 0)
			return nullptr;

		buffer_size += nbytes;
	} while (buffer_size < size);

	return buffer;
}

size_t
PeekReader::Read(void *data, size_t size)
{
	size_t buffer_remaining = buffer_size - buffer_position;
	if (buffer_remaining > 0) {
		size_t nbytes = std::min(buffer_remaining, size);
		memcpy(data, buffer + buffer_position, nbytes);
		buffer_position += nbytes;
		return nbytes;
	}

	return next.Read(data, size);
}
