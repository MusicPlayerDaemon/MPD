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

#include "MakeTag.hxx"
#include "song/TagSongFilter.hxx"
#include "song/LightSong.hxx"
#include "tag/Type.h"

#include <gtest/gtest.h>

static bool
InvokeFilter(const TagSongFilter &f, const Tag &tag) noexcept
{
	return f.Match(LightSong("dummy", tag));
}

TEST(TagSongFilter, Basic)
{
	const TagSongFilter f(TAG_TITLE,
			      StringFilter("needle", false, false, false));

	EXPECT_TRUE(InvokeFilter(f, MakeTag(TAG_TITLE, "needle")));
	EXPECT_TRUE(InvokeFilter(f, MakeTag(TAG_TITLE, "foo", TAG_TITLE, "needle")));
	EXPECT_TRUE(InvokeFilter(f, MakeTag(TAG_TITLE, "needle", TAG_TITLE, "foo")));
	EXPECT_TRUE(InvokeFilter(f, MakeTag(TAG_ARTIST, "foo", TAG_TITLE, "needle", TAG_ALBUM, "bar")));

	EXPECT_FALSE(InvokeFilter(f, MakeTag()));
	EXPECT_FALSE(InvokeFilter(f, MakeTag(TAG_TITLE, "foo")));
	EXPECT_FALSE(InvokeFilter(f, MakeTag(TAG_TITLE, "foo", TAG_TITLE, "bar")));
	EXPECT_FALSE(InvokeFilter(f, MakeTag(TAG_TITLE, "foo", TAG_ARTIST, "needle")));
	EXPECT_FALSE(InvokeFilter(f, MakeTag(TAG_TITLE, "FOOneedleBAR")));
}

/**
 * Test with empty string.  This matches tags where the given tag type
 * does not exist.
 */
TEST(TagSongFilter, Empty)
{
	const TagSongFilter f(TAG_TITLE,
			      StringFilter("", false, false, false));

	EXPECT_TRUE(InvokeFilter(f, MakeTag()));

	EXPECT_FALSE(InvokeFilter(f, MakeTag(TAG_TITLE, "foo")));
	EXPECT_FALSE(InvokeFilter(f, MakeTag(TAG_TITLE, "foo", TAG_TITLE, "bar")));
}

TEST(TagSongFilter, Substring)
{
	const TagSongFilter f(TAG_TITLE,
			      StringFilter("needle", false, true, false));

	EXPECT_TRUE(InvokeFilter(f, MakeTag(TAG_TITLE, "needle")));
	EXPECT_TRUE(InvokeFilter(f, MakeTag(TAG_TITLE, "needleBAR")));
	EXPECT_TRUE(InvokeFilter(f, MakeTag(TAG_TITLE, "FOOneedle")));
	EXPECT_TRUE(InvokeFilter(f, MakeTag(TAG_TITLE, "FOOneedleBAR")));

	EXPECT_FALSE(InvokeFilter(f, MakeTag()));
	EXPECT_FALSE(InvokeFilter(f, MakeTag(TAG_TITLE, "eedle")));
}

TEST(TagSongFilter, Negated)
{
	const TagSongFilter f(TAG_TITLE,
			      StringFilter("needle", false, false, true));

	EXPECT_TRUE(InvokeFilter(f, MakeTag()));
	EXPECT_FALSE(InvokeFilter(f, MakeTag(TAG_TITLE, "needle")));
	EXPECT_TRUE(InvokeFilter(f, MakeTag(TAG_TITLE, "foo")));
}

/**
 * Combine the "Empty" and "Negated" tests.
 */
TEST(TagSongFilter, EmptyNegated)
{
	const TagSongFilter f(TAG_TITLE,
			      StringFilter("", false, false, true));

	EXPECT_FALSE(InvokeFilter(f, MakeTag()));
	EXPECT_TRUE(InvokeFilter(f, MakeTag(TAG_TITLE, "foo")));
}

/**
 * Negation with multiple tag values.
 */
TEST(TagSongFilter, MultiNegated)
{
	const TagSongFilter f(TAG_TITLE,
			      StringFilter("needle", false, false, true));

	EXPECT_TRUE(InvokeFilter(f, MakeTag(TAG_TITLE, "foo", TAG_TITLE, "bar")));
	EXPECT_FALSE(InvokeFilter(f, MakeTag(TAG_TITLE, "needle", TAG_TITLE, "bar")));
	EXPECT_FALSE(InvokeFilter(f, MakeTag(TAG_TITLE, "foo", TAG_TITLE, "needle")));
}

/**
 * Check whether fallback tags work, e.g. AlbumArtist falls back to
 * just Artist if there is no AlbumArtist.
 */
TEST(TagSongFilter, Fallback)
{
	const TagSongFilter f(TAG_ALBUM_ARTIST,
			      StringFilter("needle", false, false, false));

	EXPECT_TRUE(InvokeFilter(f, MakeTag(TAG_ALBUM_ARTIST, "needle")));
	EXPECT_TRUE(InvokeFilter(f, MakeTag(TAG_ARTIST, "needle")));

	EXPECT_FALSE(InvokeFilter(f, MakeTag()));
	EXPECT_FALSE(InvokeFilter(f, MakeTag(TAG_ALBUM_ARTIST, "foo")));
	EXPECT_FALSE(InvokeFilter(f, MakeTag(TAG_ARTIST, "foo")));

	/* no fallback, thus the Artist tag isn't used and this must
	   be a mismatch */
	EXPECT_FALSE(InvokeFilter(f, MakeTag(TAG_ARTIST, "needle", TAG_ALBUM_ARTIST, "foo")));
}

/**
 * Combine the "Empty" and "Fallback" tests.
 */
TEST(TagSongFilter, EmptyFallback)
{
	const TagSongFilter f(TAG_ALBUM_ARTIST,
			      StringFilter("", false, false, false));

	EXPECT_TRUE(InvokeFilter(f, MakeTag()));

	EXPECT_FALSE(InvokeFilter(f, MakeTag(TAG_ALBUM_ARTIST, "foo")));
	EXPECT_FALSE(InvokeFilter(f, MakeTag(TAG_ARTIST, "foo")));
}

/**
 * Combine the "Negated" and "Fallback" tests.
 */
TEST(TagSongFilter, NegatedFallback)
{
	const TagSongFilter f(TAG_ALBUM_ARTIST,
			      StringFilter("needle", false, false, true));

	EXPECT_TRUE(InvokeFilter(f, MakeTag()));
	EXPECT_TRUE(InvokeFilter(f, MakeTag(TAG_ALBUM_ARTIST, "foo")));
	EXPECT_FALSE(InvokeFilter(f, MakeTag(TAG_ALBUM_ARTIST, "needle")));
	EXPECT_TRUE(InvokeFilter(f, MakeTag(TAG_ARTIST, "foo")));
	EXPECT_FALSE(InvokeFilter(f, MakeTag(TAG_ARTIST, "needle")));
	EXPECT_TRUE(InvokeFilter(f, MakeTag(TAG_ARTIST, "needle", TAG_ALBUM_ARTIST, "foo")));
}
