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

#include "config.h"
#include "ApeLoader.hxx"
#include "system/ByteOrder.hxx"
#include "fs/FileSystem.hxx"

#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct ape_footer {
	unsigned char id[8];
	uint32_t version;
	uint32_t length;
	uint32_t count;
	unsigned char flags[4];
	unsigned char reserved[8];
};

static bool
ape_scan_internal(FILE *fp, ApeTagCallback callback)
{
	/* determine if file has an apeV2 tag */
	struct ape_footer footer;
	if (fseek(fp, -(long)sizeof(footer), SEEK_END) ||
	    fread(&footer, 1, sizeof(footer), fp) != sizeof(footer) ||
	    memcmp(footer.id, "APETAGEX", sizeof(footer.id)) != 0 ||
	    FromLE32(footer.version) != 2000)
		return false;

	/* find beginning of ape tag */
	size_t remaining = FromLE32(footer.length);
	if (remaining <= sizeof(footer) + 10 ||
	    /* refuse to load more than one megabyte of tag data */
	    remaining > 1024 * 1024 ||
	    fseek(fp, -(long)remaining, SEEK_END))
		return false;

	/* read tag into buffer */
	remaining -= sizeof(footer);
	assert(remaining > 10);

	char *buffer = new char[remaining];
	if (fread(buffer, 1, remaining, fp) != remaining) {
		delete[] buffer;
		return false;
	}

	/* read tags */
	unsigned n = FromLE32(footer.count);
	const char *p = buffer;
	while (n-- && remaining > 10) {
		size_t size = FromLE32(*(const uint32_t *)p);
		p += 4;
		remaining -= 4;
		unsigned long flags = FromLE32(*(const uint32_t *)p);
		p += 4;
		remaining -= 4;

		/* get the key */
		const char *key = p;
		while (remaining > size && *p != '\0') {
			p++;
			remaining--;
		}
		p++;
		remaining--;

		/* get the value */
		if (remaining < size)
			break;

		if (!callback(flags, key, p, size))
			break;

		p += size;
		remaining -= size;
	}

	delete[] buffer;
	return true;
}

bool
tag_ape_scan(Path path_fs, ApeTagCallback callback)
{
	FILE *fp = FOpen(path_fs, "rb");
	if (fp == nullptr)
		return false;

	bool success = ape_scan_internal(fp, callback);
	fclose(fp);
	return success;
}
