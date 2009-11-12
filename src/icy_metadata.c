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
#include "icy_metadata.h"
#include "tag.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "icy_metadata"

void
icy_deinit(struct icy_metadata *im)
{
	if (!icy_defined(im))
		return;

	if (im->data_rest == 0 && im->meta_size > 0)
		g_free(im->meta_data);

	if (im->tag != NULL)
		tag_free(im->tag);
}

void
icy_reset(struct icy_metadata *im)
{
	if (!icy_defined(im))
		return;

	icy_deinit(im);

	im->data_rest = im->data_size;
	im->meta_size = 0;
}

size_t
icy_data(struct icy_metadata *im, size_t length)
{
	assert(length > 0);

	if (!icy_defined(im))
		return length;

	if (im->data_rest == 0)
		return 0;

	if (length >= im->data_rest) {
		length = im->data_rest;
		im->data_rest = 0;
	} else
		im->data_rest -= length;

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

	if (p[0] != NULL && p[1] != NULL) {
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

	for (unsigned i = 0; items[i] != NULL; ++i)
		icy_parse_tag_item(tag, items[i]);

	g_strfreev(items);

	return tag;
}

size_t
icy_meta(struct icy_metadata *im, const void *data, size_t length)
{
	const unsigned char *p = data;

	assert(icy_defined(im));
	assert(im->data_rest == 0);
	assert(length > 0);

	if (im->meta_size == 0) {
		/* read meta_size from the first byte of a meta
		   block */
		im->meta_size = *p++ * 16;
		if (im->meta_size == 0) {
			/* special case: no metadata */
			im->data_rest = im->data_size;
			return 1;
		}

		/* 1 byte was consumed (must be re-added later for the
		   return value */
		--length;

		/* initialize metadata reader, allocate enough
		   memory (+1 for the null terminator) */
		im->meta_position = 0;
		im->meta_data = g_malloc(im->meta_size + 1);
	}

	assert(im->meta_position < im->meta_size);

	if (length > im->meta_size - im->meta_position)
		length = im->meta_size - im->meta_position;

	memcpy(im->meta_data + im->meta_position, p, length);
	im->meta_position += length;

	if (p != data)
		/* re-add the first byte (which contained meta_size) */
		++length;

	if (im->meta_position == im->meta_size) {
		/* null-terminate the string */

		im->meta_data[im->meta_size] = 0;

		/* parse */

		if (im->tag != NULL)
			tag_free(im->tag);

		im->tag = icy_parse_tag(im->meta_data);
		g_free(im->meta_data);

		/* change back to normal data mode */

		im->meta_size = 0;
		im->data_rest = im->data_size;
	}

	return length;
}
