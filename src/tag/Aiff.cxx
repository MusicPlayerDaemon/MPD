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

#include "config.h" /* must be first for large file support */
#include "Aiff.hxx"
#include "util/Domain.hxx"
#include "system/ByteOrder.hxx"
#include "Log.hxx"

#include <limits>

#include <stdint.h>
#include <sys/stat.h>
#include <string.h>

static constexpr Domain aiff_domain("aiff");

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
aiff_seek_id3(FILE *file)
{
	/* determine the file size */

	struct stat st;
	if (fstat(fileno(file), &st) < 0) {
		LogErrno(aiff_domain, "Failed to stat file descriptor");
		return 0;
	}

	/* seek to the beginning and read the AIFF header */

	if (fseek(file, 0, SEEK_SET) != 0) {
		LogErrno(aiff_domain, "Failed to seek");
		return 0;
	}

	aiff_header header;
	size_t size = fread(&header, sizeof(header), 1, file);
	if (size != 1 ||
	    memcmp(header.id, "FORM", 4) != 0 ||
	    FromBE32(header.size) > (uint32_t)st.st_size ||
	    (memcmp(header.format, "AIFF", 4) != 0 &&
	     memcmp(header.format, "AIFC", 4) != 0))
		/* not a AIFF file */
		return 0;

	while (true) {
		/* read the chunk header */

		aiff_chunk_header chunk;
		size = fread(&chunk, sizeof(chunk), 1, file);
		if (size != 1)
			return 0;

		size = FromBE32(chunk.size);
		if (size > unsigned(std::numeric_limits<int>::max()))
			/* too dangerous, bail out: possible integer
			   underflow when casting to off_t */
			return 0;

		if (size % 2 != 0)
			/* pad byte */
			++size;

		if (memcmp(chunk.id, "ID3 ", 4) == 0)
			/* found it! */
			return size;

		if (fseek(file, size, SEEK_CUR) != 0)
			return 0;
	}
}
