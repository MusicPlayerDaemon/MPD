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
#include "TagHandler.hxx"
#include "TagBuilder.hxx"

#include <glib.h>

static void
add_tag_duration(unsigned seconds, void *ctx)
{
	TagBuilder &tag = *(TagBuilder *)ctx;

	tag.SetTime(seconds);
}

static void
add_tag_tag(enum tag_type type, const char *value, void *ctx)
{
	TagBuilder &tag = *(TagBuilder *)ctx;

	tag.AddItem(type, value);
}

const struct tag_handler add_tag_handler = {
	add_tag_duration,
	add_tag_tag,
	nullptr,
};

static void
full_tag_pair(const char *name, gcc_unused const char *value, void *ctx)
{
	TagBuilder &tag = *(TagBuilder *)ctx;

	if (g_ascii_strcasecmp(name, "cuesheet") == 0)
		tag.SetHasPlaylist(true);
}

const struct tag_handler full_tag_handler = {
	add_tag_duration,
	add_tag_tag,
	full_tag_pair,
};

