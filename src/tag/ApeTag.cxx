/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "ApeTag.hxx"
#include "ApeLoader.hxx"
#include "Tag.hxx"
#include "TagTable.hxx"
#include "TagHandler.hxx"

#include <string>

#include <string.h>

const struct tag_table ape_tags[] = {
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
 * @return true if the item was recognized
 */
static bool
tag_ape_import_item(unsigned long flags,
		    const char *key, const char *value, size_t value_length,
		    const struct tag_handler *handler, void *handler_ctx)
{
	/* we only care about utf-8 text tags */
	if ((flags & (0x3 << 1)) != 0)
		return false;

	tag_handler_invoke_pair(handler, handler_ctx, key, value);

	TagType type = tag_ape_name_parse(key);
	if (type == TAG_NUM_OF_ITEM_TYPES)
		return false;

	bool recognized = false;
	const char *end = value + value_length;
	while (true) {
		/* multiple values are separated by null bytes */
		const char *n = (const char *)memchr(value, 0, end - value);
		if (n != nullptr) {
			if (n > value) {
				tag_handler_invoke_tag(handler, handler_ctx,
						       type, value);
				recognized = true;
			}

			value = n + 1;
		} else {
			const std::string value2(value, end);
			tag_handler_invoke_tag(handler, handler_ctx,
					       type, value2.c_str());
			recognized = true;
			break;
		}
	}

	return recognized;
}

bool
tag_ape_scan2(const char *path_fs,
	      const struct tag_handler *handler, void *handler_ctx)
{
	bool recognized = false;

	auto callback = [handler, handler_ctx, &recognized]
		(unsigned long flags, const char *key,
		 const char *value,
		 size_t value_length) {
		recognized |= tag_ape_import_item(flags, key, value,
						  value_length,
						  handler, handler_ctx);
		return true;
	};

	return tag_ape_scan(path_fs, callback) && recognized;
}
