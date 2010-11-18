/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
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
#include "ape.h"

static const char *const ape_tag_names[TAG_NUM_OF_ITEM_TYPES] = {
	[TAG_ALBUM_ARTIST] = "album artist",
	[TAG_DATE] = "year",
};

static enum tag_type
tag_ape_name_parse(const char *name)
{
	enum tag_type type = tag_table_lookup(ape_tag_names, name);
	if (type == TAG_NUM_OF_ITEM_TYPES)
		type = tag_name_parse_i(name);

	return type;
}

static struct tag *
tag_ape_import_item(struct tag *tag, unsigned long flags,
		    const char *key, const char *value, size_t value_length)
{
	/* we only care about utf-8 text tags */
	if ((flags & (0x3 << 1)) != 0)
		return tag;

	enum tag_type type = tag_ape_name_parse(key);
	if (type == TAG_NUM_OF_ITEM_TYPES)
		return tag;

	if (tag == NULL)
		tag = tag_new();
	tag_add_item_n(tag, type, value, value_length);

	return tag;
}

struct tag_ape_ctx {
	struct tag *tag;
};

static bool
tag_ape_callback(unsigned long flags, const char *key,
		 const char *value, size_t value_length, void *_ctx)
{
	struct tag_ape_ctx *ctx = _ctx;

	ctx->tag = tag_ape_import_item(ctx->tag, flags, key,
				       value, value_length);
	return true;
}

struct tag *
tag_ape_load(const char *file)
{
	struct tag_ape_ctx ctx = { .tag = NULL };

	tag_ape_scan(file, tag_ape_callback, &ctx);
	return ctx.tag;
}
