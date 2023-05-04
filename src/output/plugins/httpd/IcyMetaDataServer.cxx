// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "IcyMetaDataServer.hxx"
#include "tag/Tag.hxx"
#include "util/TruncateString.hxx"

#include <fmt/core.h>

#include <iterator>

std::string
icy_server_metadata_header(const char *name,
			   const char *genre, const char *url,
			   const char *content_type, int metaint) noexcept
{
	return fmt::format("HTTP/1.1 200 OK\r\n"
			   "icy-notice1:<BR>This stream requires an audio player!<BR>\r\n" /* TODO */
			   "icy-notice2:MPD - The music player daemon<BR>\r\n"
			   "icy-name: {}\r\n"             /* TODO */
			   "icy-genre: {}\r\n"            /* TODO */
			   "icy-url: {}\r\n"              /* TODO */
			   "icy-pub:1\r\n"
			   "icy-metaint:{}\r\n"
			   /* TODO "icy-br:%d\r\n" */
			   "Content-Type: {}\r\n"
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

static std::string
icy_server_metadata_string(const char *stream_title,
			   const char* stream_url) noexcept
{
	// The leading n is a placeholder for the length information
	auto icy_metadata = fmt::format("nStreamTitle='{}';"
					"StreamUrl='{}';"
					/* pad 15 spaces just in case
					   the length needs to be
					   rounded up */
					"               ",
					stream_title,
					stream_url);

	size_t meta_length = icy_metadata.length();

	meta_length--; // subtract placeholder

	meta_length = meta_length / 16;

	icy_metadata[0] = meta_length;

	if (meta_length > 255)
		return {};

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

	if (icy_string.empty())
		return nullptr;

	return std::make_shared<Page>(std::span{(const std::byte *)icy_string.data(), uint8_t(icy_string[0]) * 16U + 1U});
}
