/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "config.h"
#include "Tags.hxx"
#include "tag/Table.hxx"
#include <string.h>

const struct tag_table upnp_tags[] = {
	{ "upnp:artist", TAG_ARTIST },
	{ "upnp:album", TAG_ALBUM },
	{ "upnp:originalTrackNumber", TAG_TRACK },
	{ "upnp:genre", TAG_GENRE },
	{ "dc:title", TAG_TITLE },
	{ "upnp:albumArtURI",TAG_ALBUM_URI},

	/* sentinel */
	{ nullptr, TAG_NUM_OF_ITEM_TYPES }
};

const struct mime_table mime_types[] = {
	{ "OGG", "audio/ogg" },
	{ "M4A", "audio/mp4" },
	{ "AAC", "audio/aac" },	
	{ "DSF", "audio/x-dsf" },
	{ "DFF", "audio/x-dff" },
	{ "FLAC", "audio/x-flac"},
	{ "AIFF", "audio/x-aiff"},
	{ "AIF", "audio/x-aiff"},
	{ "WAV","audio/x-wav"},
	{ "MP3","audio/mpeg"},
	{ "MP4","audio/mp4"},
	{ "WMA","audio/x-ms-wma"},
	{ "WAV","audio/wav"},
	{ "ASF","video/x-ms-asf"},
	{nullptr,nullptr}
};

const char *
mime_table_lookup(const mime_table *table, const char *mime)
{
	for (; table->mime_name != nullptr; ++table)
		if (strcmp(table->mime_name,mime) == 0)
			return table->suffix;

	return nullptr;
}
