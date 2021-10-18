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

#include "TagPrint.hxx"
#include "tag/Tag.hxx"
#include "tag/Settings.hxx"
#include "client/Response.hxx"
#include "util/StringView.hxx"

#include <fmt/format.h>

void
tag_print_types(Response &r) noexcept
{
	const auto tag_mask = global_tag_mask & r.GetTagMask();
	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++)
		if (tag_mask.Test(TagType(i)))
			r.Fmt(FMT_STRING("tagtype: {}\n"), tag_item_names[i]);
}

void
tag_print(Response &r, TagType type, StringView value) noexcept
{
	r.Fmt(FMT_STRING("{}: {}\n"), tag_item_names[type], value);
}

void
tag_print(Response &r, TagType type, const char *value) noexcept
{
	r.Fmt(FMT_STRING("{}: {}\n"), tag_item_names[type], value);
}

void
tag_print_values(Response &r, const Tag &tag) noexcept
{
	const auto tag_mask = r.GetTagMask();
	for (const auto &i : tag)
		if (tag_mask.Test(i.type))
			tag_print(r, i.type, i.value);
}

void
tag_print(Response &r, const Tag &tag) noexcept
{
	if (!tag.duration.IsNegative())
		r.Fmt(FMT_STRING("Time: {}\n"
				 "duration: {:1.3f}\n"),
		      tag.duration.RoundS(),
		      tag.duration.ToDoubleS());

	tag_print_values(r, tag);
}
