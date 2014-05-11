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
#include "IcyMetaDataParser.hxx"
#include "tag/Tag.hxx"
#include "tag/TagBuilder.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <assert.h>
#include <string.h>

static constexpr Domain icy_metadata_domain("icy_metadata");

void
IcyMetaDataParser::Reset()
{
	if (!IsDefined())
		return;

	if (data_rest == 0 && meta_size > 0)
		delete[] meta_data;

	delete tag;

	data_rest = data_size;
	meta_size = 0;
}

size_t
IcyMetaDataParser::Data(size_t length)
{
	assert(length > 0);

	if (!IsDefined())
		return length;

	if (data_rest == 0)
		return 0;

	if (length >= data_rest) {
		length = data_rest;
		data_rest = 0;
	} else
		data_rest -= length;

	return length;
}

static void
icy_add_item(TagBuilder &tag, TagType type, const char *value)
{
	size_t length = strlen(value);

	if (length >= 2 && value[0] == '\'' && value[length - 1] == '\'') {
		/* strip the single quotes */
		++value;
		length -= 2;
	}

	if (length > 0)
		tag.AddItem(type, value, length);
}

static void
icy_parse_tag_item(TagBuilder &tag, const char *name, const char *value)
{
	if (strcmp(name, "StreamTitle") == 0)
		icy_add_item(tag, TAG_TITLE, value);
	else
		FormatDebug(icy_metadata_domain,
			    "unknown icy-tag: '%s'", name);
}

/**
 * Find a single quote that is followed by a semicolon (or by the end
 * of the string).  If that fails, return the first single quote.  If
 * that also fails, return #end.
 */
static char *
find_end_quote(char *p, char *const end)
{
	char *fallback = std::find(p, end, '\'');
	if (fallback >= end - 1 || fallback[1] == ';')
		return fallback;

	p = fallback + 1;
	while (true) {
		p = std::find(p, end, '\'');
		if (p == end)
			return fallback;

		if (p == end - 1 || p[1] == ';')
			return p;

		++p;
	}
}

static Tag *
icy_parse_tag(char *p, char *const end)
{
	assert(p != nullptr);
	assert(end != nullptr);
	assert(p <= end);

	TagBuilder tag;

	while (p != end) {
		const char *const name = p;
		char *eq = std::find(p, end, '=');
		if (eq == end)
			break;

		*eq = 0;
		p = eq + 1;

		if (*p != '\'') {
			/* syntax error; skip to the next semicolon,
			   try to recover */
			char *semicolon = std::find(p, end, ';');
			if (semicolon == end)
				break;
			p = semicolon + 1;
			continue;
		}

		++p;

		const char *const value = p;
		char *quote = find_end_quote(p, end);
		if (quote == end)
			break;

		*quote = 0;
		p = quote + 1;

		icy_parse_tag_item(tag, name, value);

		char *semicolon = std::find(p, end, ';');
		if (semicolon == end)
			break;
		p = semicolon + 1;
	}

	return tag.CommitNew();
}

size_t
IcyMetaDataParser::Meta(const void *data, size_t length)
{
	const unsigned char *p = (const unsigned char *)data;

	assert(IsDefined());
	assert(data_rest == 0);
	assert(length > 0);

	if (meta_size == 0) {
		/* read meta_size from the first byte of a meta
		   block */
		meta_size = *p++ * 16;
		if (meta_size == 0) {
			/* special case: no metadata */
			data_rest = data_size;
			return 1;
		}

		/* 1 byte was consumed (must be re-added later for the
		   return value */
		--length;

		/* initialize metadata reader, allocate enough
		   memory (+1 for the null terminator) */
		meta_position = 0;
		meta_data = new char[meta_size + 1];
	}

	assert(meta_position < meta_size);

	if (length > meta_size - meta_position)
		length = meta_size - meta_position;

	memcpy(meta_data + meta_position, p, length);
	meta_position += length;

	if (p != data)
		/* re-add the first byte (which contained meta_size) */
		++length;

	if (meta_position == meta_size) {
		/* parse */

		delete tag;

		tag = icy_parse_tag(meta_data, meta_data + meta_size);
		delete[] meta_data;

		/* change back to normal data mode */

		meta_size = 0;
		data_rest = data_size;
	}

	return length;
}

size_t
IcyMetaDataParser::ParseInPlace(void *data, size_t length)
{
	uint8_t *const dest0 = (uint8_t *)data;
	uint8_t *dest = dest0;
	const uint8_t *src = dest0;

	while (length > 0) {
		size_t chunk = Data(length);
		if (chunk > 0) {
			memmove(dest, src, chunk);
			dest += chunk;
			src += chunk;
			length -= chunk;

			if (length == 0)
				break;
		}

		chunk = Meta(src, length);
		if (chunk > 0) {
			src += chunk;
			length -= chunk;
		}
	}

	return dest - dest0;
}
