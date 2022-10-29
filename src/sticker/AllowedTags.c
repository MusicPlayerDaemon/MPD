/*
 * Copyright 2003-2022 The Music Player Daemon Project
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


#include "AllowedTags.h"
#include "tag/Type.h"

const bool sticker_allowed_tags[TAG_NUM_OF_ITEM_TYPES] = {
	[TAG_ARTIST] = true,
	[TAG_ARTIST_SORT] = false,
	[TAG_ALBUM] = true,
	[TAG_ALBUM_SORT] = false,
	[TAG_ALBUM_ARTIST] = true,
	[TAG_ALBUM_ARTIST_SORT] = false,
	[TAG_TITLE] = true,
	[TAG_TITLE_SORT] = false,
	[TAG_TRACK] = false,
	[TAG_NAME] = false,
	[TAG_GENRE] = true,
	[TAG_MOOD] = false,
	[TAG_DATE] = false,
	[TAG_ORIGINAL_DATE] = false,
	[TAG_COMPOSER] = true,
	[TAG_COMPOSERSORT] = false,
	[TAG_PERFORMER] = true,
	[TAG_CONDUCTOR] = true,
	[TAG_WORK] = true,
	[TAG_MOVEMENT] = false,
	[TAG_MOVEMENTNUMBER] = false,
	[TAG_ENSEMBLE] = true,
	[TAG_LOCATION] = true,
	[TAG_GROUPING] = false,
	[TAG_COMMENT] = false,
	[TAG_DISC] = false,
	[TAG_LABEL] = true,

	/* MusicBrainz tags from http://musicbrainz.org/doc/MusicBrainzTag */
	[TAG_MUSICBRAINZ_ARTISTID] = true,
	[TAG_MUSICBRAINZ_ALBUMID] = true,
	[TAG_MUSICBRAINZ_ALBUMARTISTID] = true,
	[TAG_MUSICBRAINZ_TRACKID] = false,
	[TAG_MUSICBRAINZ_RELEASETRACKID] = true,
	[TAG_MUSICBRAINZ_WORKID] = true,
};
