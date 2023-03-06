// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "TagPrint.hxx"
#include "tag/Names.hxx"
#include "tag/Tag.hxx"
#include "tag/Settings.hxx"
#include "client/Response.hxx"

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
tag_print(Response &r, TagType type, std::string_view value) noexcept
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
