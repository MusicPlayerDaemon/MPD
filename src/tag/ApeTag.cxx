/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#include "ApeTag.hxx"
#include "ApeLoader.hxx"
#include "ParseName.hxx"
#include "Table.hxx"
#include "Handler.hxx"
#include "util/StringView.hxx"

#include <string>

#include <string.h>

static constexpr struct tag_table ape_tags[] = {
	{ "album artist", TAG_ALBUM_ARTIST },
	{ "year", TAG_DATE },
	{ nullptr, TAG_NUM_OF_ITEM_TYPES }
};

static TagType
tag_ape_name_parse(const char *name)
{
	TagType type = tag_table_lookup_i(ape_tags, name);
	if (type == TAG_NUM_OF_ITEM_TYPES)
		type = tag_name_parse_i(name);

	return type;
}

/**
 * Invoke the given callback for each string inside the given range.
 * The strings are separated by null bytes, but the last one may not
 * be terminated.
 */
template<typename C>
static void
ForEachValue(const char *value, const char *end, C &&callback)
{
	while (true) {
		const char *n = (const char *)memchr(value, 0, end - value);
		if (n == nullptr)
			break;

		if (n > value)
			callback(value);

		value = n + 1;
	}

	if (value < end) {
		const std::string value2(value, end);
		callback(value2.c_str());
	}
}

/**
 * @return true if the item was recognized
 */
static bool
tag_ape_import_item(unsigned long flags,
		    const char *key, StringView value,
		    TagHandler &handler) noexcept
{
	/* we only care about utf-8 text tags */
	if ((flags & (0x3 << 1)) != 0)
		return false;

	const auto begin = value.begin();
	const auto end = value.end();

	if (handler.WantPair())
		ForEachValue(begin, end, [&handler, key](const char *_value) {
				handler.OnPair(key, _value);
			});

	TagType type = tag_ape_name_parse(key);
	if (type == TAG_NUM_OF_ITEM_TYPES)
		return false;

	ForEachValue(begin, end, [&handler, type](const char *_value) {
			handler.OnTag(type, _value);
		});

	return true;
}

bool
tag_ape_scan2(InputStream &is, TagHandler &handler) noexcept
{
	bool recognized = false;

	auto callback = [&handler, &recognized]
		(unsigned long flags, const char *key,
		 StringView value) {
		recognized |= tag_ape_import_item(flags, key, value, handler);
		return true;
	};

	return tag_ape_scan(is, callback) && recognized;
}
