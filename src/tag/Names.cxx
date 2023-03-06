// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Names.hxx"
#include "Table.hxx"

#include <cassert>

static constexpr struct tag_table tag_item_names_init[] = {
	{"Artist", TAG_ARTIST},
	{"ArtistSort", TAG_ARTIST_SORT},
	{"Album", TAG_ALBUM},
	{"AlbumSort", TAG_ALBUM_SORT},
	{"AlbumArtist", TAG_ALBUM_ARTIST},
	{"AlbumArtistSort", TAG_ALBUM_ARTIST_SORT},
	{"Title", TAG_TITLE},
	{"TitleSort", TAG_TITLE_SORT},
	{"Track", TAG_TRACK},
	{"Name", TAG_NAME},
	{"Genre", TAG_GENRE},
	{"Mood", TAG_MOOD},
	{"Date", TAG_DATE},
	{"OriginalDate", TAG_ORIGINAL_DATE},
	{"Composer", TAG_COMPOSER},
	{"ComposerSort", TAG_COMPOSERSORT},
	{"Performer", TAG_PERFORMER},
	{"Conductor", TAG_CONDUCTOR},
	{"Work", TAG_WORK},
	{"Movement", TAG_MOVEMENT},
	{"MovementNumber", TAG_MOVEMENTNUMBER},
	{"Ensemble", TAG_ENSEMBLE},
	{"Location", TAG_LOCATION},
	{"Grouping", TAG_GROUPING},
	{"Comment", TAG_COMMENT},
	{"Disc", TAG_DISC},
	{"Label", TAG_LABEL},

	/* MusicBrainz tags from http://musicbrainz.org/doc/MusicBrainzTag */
	{"MUSICBRAINZ_ARTISTID", TAG_MUSICBRAINZ_ARTISTID},
	{"MUSICBRAINZ_ALBUMID", TAG_MUSICBRAINZ_ALBUMID},
	{"MUSICBRAINZ_ALBUMARTISTID", TAG_MUSICBRAINZ_ALBUMARTISTID},
	{"MUSICBRAINZ_TRACKID", TAG_MUSICBRAINZ_TRACKID},
	{"MUSICBRAINZ_RELEASETRACKID", TAG_MUSICBRAINZ_RELEASETRACKID},
	{"MUSICBRAINZ_WORKID", TAG_MUSICBRAINZ_WORKID},
};

/**
 * This function converts the #tag_item_names_init array to an
 * associative array at compile time.  This is a kludge because C++20
 * doesn't support designated initializers for arrays, unlike C99.
 */
static constexpr auto
MakeTagNames() noexcept
{
	std::array<const char *, TAG_NUM_OF_ITEM_TYPES> result{};

	static_assert(std::size(tag_item_names_init) == result.size());

	for (const auto &i : tag_item_names_init) {
		/* no duplicates allowed */
		assert(result[i.type] == nullptr);

		result[i.type] = i.name;
	}

	return result;
}

constinit const std::array<const char *, TAG_NUM_OF_ITEM_TYPES> tag_item_names = MakeTagNames();
