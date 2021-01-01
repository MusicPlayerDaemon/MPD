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

#include "Id3MusicBrainz.hxx"
#include "Table.hxx"
#include "Type.h"

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
	{ nullptr, TAG_NUM_OF_ITEM_TYPES }
};
