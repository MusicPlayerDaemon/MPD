/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "Aiff.hxx"
#include "input/InputStream.hxx"
#include "system/ByteOrder.hxx"
#include "Log.hxx"
#include "util/Error.hxx"

#include <limits>

#include <stdint.h>
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
aiff_seek_id3(InputStream &is)
{
	/* seek to the beginning and read the AIFF header */

	Error error;
	if (!is.Rewind(error)) {
		LogError(error, "Failed to seek");
		return 0;
	}

	aiff_header header;
	if (!is.ReadFull(&header, sizeof(header), IgnoreError()) ||
	    memcmp(header.id, "FORM", 4) != 0 ||
	    (is.KnownSize() && FromLE32(header.size) > is.GetSize()) ||
	    (memcmp(header.format, "AIFF", 4) != 0 &&
	     memcmp(header.format, "AIFC", 4) != 0))
		/* not a AIFF file */
		return 0;

	while (true) {
		/* read the chunk header */

		aiff_chunk_header chunk;
		if (!is.ReadFull(&chunk, sizeof(chunk), IgnoreError()))
			return 0;

		size_t size = FromBE32(chunk.size);
		if (size > size_t(std::numeric_limits<int>::max()))
			/* too dangerous, bail out: possible integer
			   underflow when casting to off_t */
			return 0;

		if (memcmp(chunk.id, "ID3 ", 4) == 0)
			/* found it! */
			return size;

		if (size % 2 != 0)
			/* pad byte */
			++size;

		if (!is.Skip(size, IgnoreError()))
			return 0;
	}
}
