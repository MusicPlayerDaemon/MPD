// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Type.h"

const char *const tag_item_names[TAG_NUM_OF_ITEM_TYPES] = {
	[TAG_ARTIST] = "Artist",
	[TAG_ARTIST_SORT] = "ArtistSort",
	[TAG_ALBUM] = "Album",
	[TAG_ALBUM_SORT] = "AlbumSort",
	[TAG_ALBUM_ARTIST] = "AlbumArtist",
	[TAG_ALBUM_ARTIST_SORT] = "AlbumArtistSort",
	[TAG_TITLE] = "Title",
	[TAG_TITLE_SORT] = "TitleSort",
	[TAG_TRACK] = "Track",
	[TAG_NAME] = "Name",
	[TAG_GENRE] = "Genre",
	[TAG_MOOD] = "Mood",
	[TAG_DATE] = "Date",
	[TAG_ORIGINAL_DATE] = "OriginalDate",
	[TAG_COMPOSER] = "Composer",
	[TAG_COMPOSERSORT] = "ComposerSort",
	[TAG_PERFORMER] = "Performer",
	[TAG_CONDUCTOR] = "Conductor",
	[TAG_WORK] = "Work",
	[TAG_MOVEMENT] = "Movement",
	[TAG_MOVEMENTNUMBER] = "MovementNumber",
	[TAG_ENSEMBLE] = "Ensemble",
	[TAG_LOCATION] = "Location",
	[TAG_GROUPING] = "Grouping",
	[TAG_COMMENT] = "Comment",
	[TAG_DISC] = "Disc",
	[TAG_LABEL] = "Label",

	/* MusicBrainz tags from http://musicbrainz.org/doc/MusicBrainzTag */
	[TAG_MUSICBRAINZ_ARTISTID] = "MUSICBRAINZ_ARTISTID",
	[TAG_MUSICBRAINZ_ALBUMID] = "MUSICBRAINZ_ALBUMID",
	[TAG_MUSICBRAINZ_ALBUMARTISTID] = "MUSICBRAINZ_ALBUMARTISTID",
	[TAG_MUSICBRAINZ_TRACKID] = "MUSICBRAINZ_TRACKID",
	[TAG_MUSICBRAINZ_RELEASETRACKID] = "MUSICBRAINZ_RELEASETRACKID",
	[TAG_MUSICBRAINZ_WORKID] = "MUSICBRAINZ_WORKID",
};
