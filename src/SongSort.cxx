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
#include "SongSort.hxx"
#include "Song.hxx"
#include "util/list.h"
#include "tag/Tag.hxx"

extern "C" {
#include "util/list_sort.h"
}

#include <glib.h>

#include <assert.h>
#include <stdlib.h>

static const char *
tag_get_value_checked(const Tag *tag, enum tag_type type)
{
	return tag != nullptr
		? tag->GetValue(type)
		: nullptr;
}

static int
compare_utf8_string(const char *a, const char *b)
{
	if (a == nullptr)
		return b == nullptr ? 0 : -1;

	if (b == nullptr)
		return 1;

	return g_utf8_collate(a, b);
}

/**
 * Compare two string tag values, ignoring case.  Either one may be
 * nullptr.
 */
static int
compare_string_tag_item(const Tag *a, const Tag *b,
			enum tag_type type)
{
	return compare_utf8_string(tag_get_value_checked(a, type),
				   tag_get_value_checked(b, type));
}

/**
 * Compare two tag values which should contain an integer value
 * (e.g. disc or track number).  Either one may be nullptr.
 */
static int
compare_number_string(const char *a, const char *b)
{
	long ai = a == nullptr ? 0 : strtol(a, nullptr, 10);
	long bi = b == nullptr ? 0 : strtol(b, nullptr, 10);

	if (ai <= 0)
		return bi <= 0 ? 0 : -1;

	if (bi <= 0)
		return 1;

	return ai - bi;
}

static int
compare_tag_item(const Tag *a, const Tag *b, enum tag_type type)
{
	return compare_number_string(tag_get_value_checked(a, type),
				     tag_get_value_checked(b, type));
}

/* Only used for sorting/searchin a songvec, not general purpose compares */
static int
song_cmp(gcc_unused void *priv, struct list_head *_a, struct list_head *_b)
{
	const Song *a = (const Song *)_a;
	const Song *b = (const Song *)_b;
	int ret;

	/* first sort by album */
	ret = compare_string_tag_item(a->tag, b->tag, TAG_ALBUM);
	if (ret != 0)
		return ret;

	/* then sort by disc */
	ret = compare_tag_item(a->tag, b->tag, TAG_DISC);
	if (ret != 0)
		return ret;

	/* then by track number */
	ret = compare_tag_item(a->tag, b->tag, TAG_TRACK);
	if (ret != 0)
		return ret;

	/* still no difference?  compare file name */
	return g_utf8_collate(a->uri, b->uri);
}

void
song_list_sort(struct list_head *songs)
{
	list_sort(nullptr, songs, song_cmp);
}
