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

#include "Aiff.hxx"
#include "input/InputStream.hxx"
#include "util/ByteOrder.hxx"

#include <cstdint>
#include <limits>
#include <stdexcept>

#include <string.h>

struct aiff_header {
	char id[4];
	uint32_t size;
	char format[4];
};

struct aiff_chunk_header {
	char id[4];
	uint32_t size;
};

size_t
aiff_seek_id3(InputStream &is, std::unique_lock<Mutex> &lock)
{
	/* seek to the beginning and read the AIFF header */

	is.Rewind(lock);

	aiff_header header;
	is.ReadFull(lock, &header, sizeof(header));
	if (memcmp(header.id, "FORM", 4) != 0 ||
	    (is.KnownSize() && FromBE32(header.size) > is.GetSize()) ||
	    (memcmp(header.format, "AIFF", 4) != 0 &&
	     memcmp(header.format, "AIFC", 4) != 0))
		throw std::runtime_error("Not an AIFF file");

	while (true) {
		/* read the chunk header */

		aiff_chunk_header chunk;
		is.ReadFull(lock, &chunk, sizeof(chunk));

		size_t size = FromBE32(chunk.size);
		if (size > size_t(std::numeric_limits<int>::max()))
			/* too dangerous, bail out: possible integer
			   underflow when casting to off_t */
			throw std::runtime_error("AIFF chunk is too large");

		if (memcmp(chunk.id, "ID3 ", 4) == 0)
			/* found it! */
			return size;

		if (size % 2 != 0)
			/* pad byte */
			++size;

		is.Skip(lock, size);
	}
}
