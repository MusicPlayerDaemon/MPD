/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "tag_ape.h"
#include "tag.h"
#include "tag_table.h"
#include "tag_handler.h"
#include "ape.h"

const struct tag_table ape_tags[] = {
	{ "album artist", TAG_ALBUM_ARTIST },
	{ "year", TAG_DATE },
	{ NULL, TAG_NUM_OF_ITEM_TYPES }
};

static enum tag_type
tag_ape_name_parse(const char *name)
{
	enum tag_type type = tag_table_lookup_i(ape_tags, name);
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

	enum tag_type type = tag_ape_name_parse(key);
	if (type == TAG_NUM_OF_ITEM_TYPES)
		return false;

	bool recognized = false;
	const char *end = value + value_length;
	while (true) {
		/* multiple values are separated by null bytes */
		const char *n = memchr(value, 0, end - value);
		if (n != NULL) {
			if (n > value) {
				tag_handler_invoke_tag(handler, handler_ctx,
						       type, value);
				recognized = true;
			}

			value = n + 1;
		} else {
			char *p = g_strndup(value, end - value);
			tag_handler_invoke_tag(handler, handler_ctx,
					       type, p);
			g_free(p);
			recognized = true;
			break;
		}
	}

	return recognized;
}

struct tag_ape_ctx {
	const struct tag_handler *handler;
	void *handler_ctx;

	bool recognized;
};

static bool
tag_ape_callback(unsigned long flags, const char *key,
		 const char *value, size_t value_length, void *_ctx)
{
	struct tag_ape_ctx *ctx = _ctx;

	ctx->recognized |= tag_ape_import_item(flags, key, value, value_length,
					       ctx->handler, ctx->handler_ctx);
	return true;
}

bool
tag_ape_scan2(const char *path_fs,
	      const struct tag_handler *handler, void *handler_ctx)
{
	struct tag_ape_ctx ctx = {
		.handler = handler,
		.handler_ctx = handler_ctx,
		.recognized = false,
	};

	return tag_ape_scan(path_fs, tag_ape_callback, &ctx) &&
		ctx.recognized;
}
