// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Id3MusicBrainz.hxx"
#include "Table.hxx"
#include "Type.hxx"

const struct tag_table musicbrainz_txxx_tags[] = {
	{ "ALBUMARTISTSORT", TAG_ALBUM_ARTIST_SORT },
	{ "MusicBrainz Artist Id", TAG_MUSICBRAINZ_ARTISTID },
	{ "MusicBrainz Album Id", TAG_MUSICBRAINZ_ALBUMID },
	{ "MusicBrainz Album Artist Id",
	  TAG_MUSICBRAINZ_ALBUMARTISTID },
	{ "MusicBrainz Track Id", TAG_MUSICBRAINZ_TRACKID },
	{ "MusicBrainz Release Track Id",
	  TAG_MUSICBRAINZ_RELEASETRACKID },
	{ "MusicBrainz Work Id", TAG_MUSICBRAINZ_WORKID },
	{ "MusicBrainz Release Group Id",
	  TAG_MUSICBRAINZ_RELEASEGROUPID },
	{ nullptr, TAG_NUM_OF_ITEM_TYPES }
};
