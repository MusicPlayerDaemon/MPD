// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "SongSort.hxx"
#include "Song.hxx"
#include "tag/Tag.hxx"
#include "lib/icu/Collate.hxx"
#include "util/IntrusiveList.hxx"
#include "util/SortList.hxx"

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
[[gnu::pure]]
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
song_list_sort(IntrusiveList<Song> &songs) noexcept
{
	SortList(songs, song_cmp);
}
