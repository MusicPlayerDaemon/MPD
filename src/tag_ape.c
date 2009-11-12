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

#include "config.h"
#include "tag_ape.h"
#include "tag.h"

#include <glib.h>

#include <assert.h>
#include <stdio.h>

static const char *const ape_tag_names[] = {
	[TAG_TITLE] = "title",
	[TAG_ARTIST] = "artist",
	[TAG_ALBUM] = "album",
	[TAG_COMMENT] = "comment",
	[TAG_GENRE] = "genre",
	[TAG_TRACK] = "track",
	[TAG_DATE] = "year"
};

static struct tag *
tag_ape_import_item(struct tag *tag, unsigned long flags,
		    const char *key, const char *value, size_t value_length)
{
	/* we only care about utf-8 text tags */
	if ((flags & (0x3 << 1)) != 0)
		return tag;

	for (unsigned i = 0; i < G_N_ELEMENTS(ape_tag_names); i++) {
		if (ape_tag_names[i] != NULL &&
		    g_ascii_strcasecmp(key, ape_tag_names[i]) == 0) {
			if (tag == NULL)
				tag = tag_new();
			tag_add_item_n(tag, i, value, value_length);
		}
	}

	return tag;
}

struct tag *
tag_ape_load(const char *file)
{
	struct tag *ret = NULL;
	FILE *fp;
	int tagCount;
	char *buffer = NULL;
	char *p;
	size_t tagLen;
	size_t size;
	unsigned long flags;
	char *key;

	struct {
		unsigned char id[8];
		uint32_t version;
		uint32_t length;
		uint32_t tagCount;
		unsigned char flags[4];
		unsigned char reserved[8];
	} footer;

	fp = fopen(file, "r");
	if (!fp)
		return NULL;

	/* determine if file has an apeV2 tag */
	if (fseek(fp, 0, SEEK_END))
		goto fail;
	size = (size_t)ftell(fp);
	if (fseek(fp, size - sizeof(footer), SEEK_SET))
		goto fail;
	if (fread(&footer, 1, sizeof(footer), fp) != sizeof(footer))
		goto fail;
	if (memcmp(footer.id, "APETAGEX", sizeof(footer.id)) != 0)
		goto fail;
	if (GUINT32_FROM_LE(footer.version) != 2000)
		goto fail;

	/* find beginning of ape tag */
	tagLen = GUINT32_FROM_LE(footer.length);
	if (tagLen <= sizeof(footer) + 10)
		goto fail;
	if (tagLen > 1024 * 1024)
		/* refuse to load more than one megabyte of tag data */
		goto fail;
	if (fseek(fp, size - tagLen, SEEK_SET))
		goto fail;

	/* read tag into buffer */
	tagLen -= sizeof(footer);
	assert(tagLen > 10);

	buffer = g_malloc(tagLen);
	if (fread(buffer, 1, tagLen, fp) != tagLen)
		goto fail;

	/* read tags */
	tagCount = GUINT32_FROM_LE(footer.tagCount);
	p = buffer;
	while (tagCount-- && tagLen > 10) {
		size = GUINT32_FROM_LE(*(const uint32_t *)p);
		p += 4;
		tagLen -= 4;
		flags = GUINT32_FROM_LE(*(const uint32_t *)p);
		p += 4;
		tagLen -= 4;

		/* get the key */
		key = p;
		while (tagLen > size && *p != '\0') {
			p++;
			tagLen--;
		}
		p++;
		tagLen--;

		/* get the value */
		if (tagLen < size)
			goto fail;

		ret = tag_ape_import_item(ret, flags, key, p, size);

		p += size;
		tagLen -= size;
	}

fail:
	if (fp)
		fclose(fp);
	g_free(buffer);
	return ret;
}
