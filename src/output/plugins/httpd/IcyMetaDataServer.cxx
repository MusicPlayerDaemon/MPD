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

#include "IcyMetaDataServer.hxx"
#include "tag/Tag.hxx"
#include "util/FormatString.hxx"
#include "util/AllocatedString.hxx"
#include "util/TruncateString.hxx"

#include <iterator>

#include <string.h>

AllocatedString
icy_server_metadata_header(const char *name,
			   const char *genre, const char *url,
			   const char *content_type, int metaint) noexcept
{
	return FormatString("HTTP/1.1 200 OK\r\n"
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
			    "Access-Control-Allow-Origin: *\r\n"
			    "\r\n",
			    name,
			    genre,
			    url,
			    metaint,
			    /* bitrate, */
			    content_type);
}

static AllocatedString
icy_server_metadata_string(const char *stream_title,
			   const char* stream_url) noexcept
{
	// The leading n is a placeholder for the length information
	auto icy_metadata = FormatString("nStreamTitle='%s';"
					 "StreamUrl='%s';"
					 /* pad 15 spaces just in case
					    the length needs to be
					    rounded up */
					 "               ",
					 stream_title,
					 stream_url);

	size_t meta_length = strlen(icy_metadata.c_str());

	meta_length--; // subtract placeholder

	meta_length = meta_length / 16;

	icy_metadata[0] = meta_length;

	if (meta_length > 255)
		return nullptr;

	return icy_metadata;
}

PagePtr
icy_server_metadata_page(const Tag &tag, const TagType *types) noexcept
{
	const char *tag_items[TAG_NUM_OF_ITEM_TYPES];

	int last_item = -1;
	while (*types != TAG_NUM_OF_ITEM_TYPES) {
		const char *tag_item = tag.GetValue(*types++);
		if (tag_item)
			tag_items[++last_item] = tag_item;
	}

	int item = 0;

	// Length + Metadata - "StreamTitle='';StreamUrl='';" = 4081 - 28
	char stream_title[(1 + 255 - 28) * 16];
	char *p = stream_title, *const end = stream_title + std::size(stream_title);
	stream_title[0] =  '\0';

	while (p < end && item <= last_item) {
		p = CopyTruncateString(p, tag_items[item++], end - p);

		if (item <= last_item)
			p = CopyTruncateString(p, " - ", end - p);
	}

	const auto icy_string = icy_server_metadata_string(stream_title, "");

	if (icy_string == nullptr)
		return nullptr;

	return std::make_shared<Page>(ConstBuffer<std::byte>{(const std::byte *)icy_string.c_str(), uint8_t(icy_string[0]) * 16U + 1U});
}
