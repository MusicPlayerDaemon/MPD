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

#include "config.h" /* must be first for large file support */
#include "Riff.hxx"
#include "input/InputStream.hxx"
#include "system/ByteOrder.hxx"
#include "Log.hxx"
#include "util/Error.hxx"

#include <limits>

#include <stdint.h>
#include <string.h>

struct riff_header {
	char id[4];
	uint32_t size;
	char format[4];
};

struct riff_chunk_header {
	char id[4];
	uint32_t size;
};

size_t
riff_seek_id3(InputStream &is)
{
	/* seek to the beginning and read the RIFF header */

	Error error;
	if (!is.Rewind(error)) {
		LogError(error, "Failed to seek");
		return 0;
	}

	riff_header header;
	if (!is.ReadFull(&header, sizeof(header), IgnoreError()) ||
	    memcmp(header.id, "RIFF", 4) != 0 ||
	    (is.KnownSize() && FromLE32(header.size) > is.GetSize()))
		/* not a RIFF file */
		return 0;

	while (true) {
		/* read the chunk header */

		riff_chunk_header chunk;
		if (!is.ReadFull(&chunk, sizeof(chunk), IgnoreError()))
			return 0;

		size_t size = FromLE32(chunk.size);
		if (size > size_t(std::numeric_limits<int>::max()))
			/* too dangerous, bail out: possible integer
			   underflow when casting to off_t */
			return 0;

		if (memcmp(chunk.id, "id3 ", 4) == 0 ||
		    memcmp(chunk.id, "ID3 ", 4) == 0)
			/* found it! */
			return size;

		if (size % 2 != 0)
			/* pad byte */
			++size;

		if (!is.Skip(size, IgnoreError()))
			return 0;
	}
}
