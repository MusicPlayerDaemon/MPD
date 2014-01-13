/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "IcyMetaDataServer.hxx"
#include "Page.hxx"
#include "tag/Tag.hxx"
#include "util/FormatString.hxx"

#include <glib.h>

#include <string.h>

char*
icy_server_metadata_header(const char *name,
			   const char *genre, const char *url,
			   const char *content_type, int metaint)
{
	return FormatNew("ICY 200 OK\r\n"
			 "icy-notice1:<BR>This stream requires an audio player!<BR>\r\n" /* TODO */
			 "icy-notice2:MPD - The music player daemon<BR>\r\n"
			 "icy-name: %s\r\n"             /* TODO */
			 "icy-genre: %s\r\n"            /* TODO */
			 "icy-url: %s\r\n"              /* TODO */
			 "icy-pub:1\r\n"
			 "icy-metaint:%d\r\n"
			 /* TODO "icy-br:%d\r\n" */
			 "Content-Type: %s\r\n"
			 "Connection: close\r\n"
			 "Pragma: no-cache\r\n"
			 "Cache-Control: no-cache, no-store\r\n"
			 "\r\n",
			 name,
			 genre,
			 url,
			 metaint,
			 /* bitrate, */
			 content_type);
}

static char *
icy_server_metadata_string(const char *stream_title, const char* stream_url)
{
	gchar *icy_metadata;
	guint meta_length;

	// The leading n is a placeholder for the length information
	icy_metadata = FormatNew("nStreamTitle='%s';"
				 "StreamUrl='%s';",
				 stream_title,
				 stream_url);

	meta_length = strlen(icy_metadata);

	meta_length--; // subtract placeholder

	meta_length = ((int)meta_length / 16) + 1;

	icy_metadata[0] = meta_length;

	if (meta_length > 255) {
		delete[] icy_metadata;
		return nullptr;
	}

	return icy_metadata;
}

Page *
icy_server_metadata_page(const Tag &tag, const TagType *types)
{
	const gchar *tag_items[TAG_NUM_OF_ITEM_TYPES];
	gint last_item, item;
	guint position;
	gchar *icy_string;
	gchar stream_title[(1 + 255 - 28) * 16]; // Length + Metadata -
						 // "StreamTitle='';StreamUrl='';"
						 // = 4081 - 28
	stream_title[0] =  '\0';

	last_item = -1;

	while (*types != TAG_NUM_OF_ITEM_TYPES) {
		const gchar *tag_item = tag.GetValue(*types++);
		if (tag_item)
			tag_items[++last_item] = tag_item;
	}

	position = item = 0;
	while (position < sizeof(stream_title) && item <= last_item) {
		gint length = 0;

		length = g_strlcpy(stream_title + position,
				   tag_items[item++],
				   sizeof(stream_title) - position);

		position += length;

		if (item <= last_item) {
			length = g_strlcpy(stream_title + position,
					   " - ",
					   sizeof(stream_title) - position);

			position += length;
		}
	}

	icy_string = icy_server_metadata_string(stream_title, "");

	if (icy_string == nullptr)
		return nullptr;

	Page *icy_metadata = Page::Copy(icy_string, (icy_string[0] * 16) + 1);

	delete[] icy_string;

	return icy_metadata;
}
