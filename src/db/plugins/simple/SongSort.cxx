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

#include "SongSort.hxx"
#include "Song.hxx"
#include "tag/Tag.hxx"
#include "lib/icu/Collate.hxx"

#include <stdlib.h>

static int
compare_utf8_string(const char *a, const char *b) noexcept
{
	if (a == nullptr)
		return b == nullptr ? 0 : -1;

	if (b == nullptr)
		return 1;

	return IcuCollate(a, b);
}

/**
 * Compare two string tag values, ignoring case.  Either one may be
 * nullptr.
 */
static int
compare_string_tag_item(const Tag &a, const Tag &b, TagType type) noexcept
{
	return compare_utf8_string(a.GetValue(type),
				   b.GetValue(type));
}

/**
 * Compare two tag values which should contain an integer value
 * (e.g. disc or track number).  Either one may be nullptr.
 */
static int
compare_number_string(const char *a, const char *b) noexcept
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
compare_tag_item(const Tag &a, const Tag &b, TagType type) noexcept
{
	return compare_number_string(a.GetValue(type),
				     b.GetValue(type));
}

/* Only used for sorting/searchin a songvec, not general purpose compares */
gcc_pure
static bool
song_cmp(const Song &a, const Song &b) noexcept
{
	int ret;

	/* first sort by album */
	ret = compare_string_tag_item(a.tag, b.tag, TAG_ALBUM);
	if (ret != 0)
		return ret < 0;

	/* then sort by disc */
	ret = compare_tag_item(a.tag, b.tag, TAG_DISC);
	if (ret != 0)
		return ret < 0;

	/* then by track number */
	ret = compare_tag_item(a.tag, b.tag, TAG_TRACK);
	if (ret != 0)
		return ret < 0;

	/* still no difference?  compare file name */
	return IcuCollate(a.filename, b.filename) < 0;
}

void
song_list_sort(SongList &songs) noexcept
{
	songs.sort(song_cmp);
}
