/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "IcyMetaDataParser.hxx"
#include "tag.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "icy_metadata"

void
IcyMetaDataParser::Reset()
{
	if (!IsDefined())
		return;

	if (data_rest == 0 && meta_size > 0)
		g_free(meta_data);

	if (tag != nullptr)
		tag_free(tag);

	data_rest = data_size;
	meta_size = 0;
}

size_t
IcyMetaDataParser::Data(size_t length)
{
	assert(length > 0);

	if (!IsDefined())
		return length;

	if (data_rest == 0)
		return 0;

	if (length >= data_rest) {
		length = data_rest;
		data_rest = 0;
	} else
		data_rest -= length;

	return length;
}

static void
icy_add_item(struct tag *tag, enum tag_type type, const char *value)
{
	size_t length = strlen(value);

	if (length >= 2 && value[0] == '\'' && value[length - 1] == '\'') {
		/* strip the single quotes */
		++value;
		length -= 2;
	}

	if (length > 0)
		tag_add_item_n(tag, type, value, length);
}

static void
icy_parse_tag_item(struct tag *tag, const char *item)
{
	gchar **p = g_strsplit(item, "=", 0);

	if (p[0] != nullptr && p[1] != nullptr) {
		if (strcmp(p[0], "StreamTitle") == 0)
			icy_add_item(tag, TAG_TITLE, p[1]);
		else
			g_debug("unknown icy-tag: '%s'", p[0]);
	}

	g_strfreev(p);
}

static struct tag *
icy_parse_tag(const char *p)
{
	struct tag *tag = tag_new();
	gchar **items = g_strsplit(p, ";", 0);

	for (unsigned i = 0; items[i] != nullptr; ++i)
		icy_parse_tag_item(tag, items[i]);

	g_strfreev(items);

	return tag;
}

size_t
IcyMetaDataParser::Meta(const void *data, size_t length)
{
	const unsigned char *p = (const unsigned char *)data;

	assert(IsDefined());
	assert(data_rest == 0);
	assert(length > 0);

	if (meta_size == 0) {
		/* read meta_size from the first byte of a meta
		   block */
		meta_size = *p++ * 16;
		if (meta_size == 0) {
			/* special case: no metadata */
			data_rest = data_size;
			return 1;
		}

		/* 1 byte was consumed (must be re-added later for the
		   return value */
		--length;

		/* initialize metadata reader, allocate enough
		   memory (+1 for the null terminator) */
		meta_position = 0;
		meta_data = (char *)g_malloc(meta_size + 1);
	}

	assert(meta_position < meta_size);

	if (length > meta_size - meta_position)
		length = meta_size - meta_position;

	memcpy(meta_data + meta_position, p, length);
	meta_position += length;

	if (p != data)
		/* re-add the first byte (which contained meta_size) */
		++length;

	if (meta_position == meta_size) {
		/* null-terminate the string */

		meta_data[meta_size] = 0;

		/* parse */

		if (tag != nullptr)
			tag_free(tag);

		tag = icy_parse_tag(meta_data);
		g_free(meta_data);

		/* change back to normal data mode */

		meta_size = 0;
		data_rest = data_size;
	}

	return length;
}
