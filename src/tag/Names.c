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

#include "Type.h"

const char *const tag_item_names[TAG_NUM_OF_ITEM_TYPES] = {
	[TAG_ARTIST] = "Artist",
	[TAG_ARTIST_SORT] = "ArtistSort",
	[TAG_ALBUM] = "Album",
	[TAG_ALBUM_SORT] = "AlbumSort",
	[TAG_ALBUM_ARTIST] = "AlbumArtist",
	[TAG_ALBUM_ARTIST_SORT] = "AlbumArtistSort",
	[TAG_TITLE] = "Title",
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
