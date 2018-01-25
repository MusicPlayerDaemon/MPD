/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "Handler.hxx"
#include "Builder.hxx"
#include "util/ASCII.hxx"
#include "util/StringFormat.hxx"

#include <stdlib.h>

static void
add_tag_duration(SongTime duration, void *ctx)
{
	TagBuilder &tag = *(TagBuilder *)ctx;

	tag.SetDuration(duration);
}

static void
add_tag_tag(TagType type, const char *value, void *ctx)
{
	TagBuilder &tag = *(TagBuilder *)ctx;

	if (type == TAG_TRACK || type == TAG_DISC) {
		/* filter out this extra data and leading zeroes */
		char *end;
		unsigned n = strtoul(value, &end, 10);
		if (value != end)
			tag.AddItem(type, StringFormat<21>("%u", n));
	} else
		tag.AddItem(type, value);
}

const TagHandler add_tag_handler = {
	add_tag_duration,
	add_tag_tag,
	nullptr,
};

static void
full_tag_pair(const char *name, gcc_unused const char *value, void *ctx)
{
	TagBuilder &tag = *(TagBuilder *)ctx;

	if (StringEqualsCaseASCII(name, "cuesheet"))
		tag.SetHasPlaylist(true);
}

const TagHandler full_tag_handler = {
	add_tag_duration,
	add_tag_tag,
	full_tag_pair,
};

