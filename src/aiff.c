/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "aiff.h"

#include <glib.h>

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "aiff"

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
	int ret;
	struct stat st;
	struct aiff_header header;
	struct aiff_chunk_header chunk;
	size_t size;

	/* determine the file size */

	ret = fstat(fileno(file), &st);
	if (ret < 0) {
		g_warning("Failed to stat file descriptor: %s",
			  strerror(errno));
		return 0;
	}

	/* seek to the beginning and read the AIFF header */

	ret = fseek(file, 0, SEEK_SET);
	if (ret != 0) {
		g_warning("Failed to seek: %s", g_strerror(errno));
		return 0;
	}

	size = fread(&header, sizeof(header), 1, file);
	if (size != 1 ||
	    memcmp(header.id, "FORM", 4) != 0 ||
	    GUINT32_FROM_BE(header.size) > (uint32_t)st.st_size ||
	    memcmp(header.format, "AIFF", 4) != 0)
		/* not a AIFF file */
		return 0;

	while (true) {
		/* read the chunk header */

		size = fread(&chunk, sizeof(chunk), 1, file);
		if (size != 1)
			return 0;

		size = GUINT32_FROM_BE(chunk.size);
		if (size > G_MAXINT32)
			/* too dangerous, bail out: possible integer
			   underflow when casting to off_t */
			return 0;

		if (size % 2 != 0)
			/* pad byte */
			++size;

		if (memcmp(chunk.id, "ID3 ", 4) == 0)
			/* found it! */
			return size;

		ret = fseek(file, size, SEEK_CUR);
		if (ret != 0)
			return 0;
	}
}
