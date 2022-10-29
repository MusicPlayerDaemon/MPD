// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "AllowedTags.hxx"
#include "tag/Mask.hxx"

#include <cassert>

static constexpr TagType sticker_allowed_tags_init[] = {
	TAG_ARTIST,
	TAG_ALBUM,
	TAG_ALBUM_ARTIST,
	TAG_TITLE,
	TAG_GENRE,
	TAG_COMPOSER,
	TAG_PERFORMER,
	TAG_CONDUCTOR,
	TAG_WORK,
	TAG_ENSEMBLE,
	TAG_LOCATION,
	TAG_LABEL,
	TAG_MUSICBRAINZ_ARTISTID,
	TAG_MUSICBRAINZ_ALBUMID,
	TAG_MUSICBRAINZ_ALBUMARTISTID,
	TAG_MUSICBRAINZ_RELEASETRACKID,
	TAG_MUSICBRAINZ_WORKID,
};

static constexpr auto
TagArrayToMask() noexcept
{
	auto result = TagMask::None();

	for (const auto i : sticker_allowed_tags_init) {
		/* no duplicates allowed */
		assert(!result.Test(i));

		result |= i;
	}

	return result;
}

constinit const TagMask sticker_allowed_tags = TagArrayToMask();
