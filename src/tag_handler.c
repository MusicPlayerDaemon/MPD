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
#include "tag_handler.h"

#include <glib.h>

static void
add_tag_duration(unsigned seconds, void *ctx)
{
	struct tag *tag = ctx;

	tag->time = seconds;
}

static void
add_tag_tag(enum tag_type type, const char *value, void *ctx)
{
	struct tag *tag = ctx;

	tag_add_item(tag, type, value);
}

const struct tag_handler add_tag_handler = {
	.duration = add_tag_duration,
	.tag = add_tag_tag,
};

static void
full_tag_pair(const char *name, G_GNUC_UNUSED const char *value, void *ctx)
{
	struct tag *tag = ctx;

	if (g_ascii_strcasecmp(name, "cuesheet") == 0)
		tag->has_playlist = true;
}

const struct tag_handler full_tag_handler = {
	.duration = add_tag_duration,
	.tag = add_tag_tag,
	.pair = full_tag_pair,
};

