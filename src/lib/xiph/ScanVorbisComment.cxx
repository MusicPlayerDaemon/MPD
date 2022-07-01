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

#include "ScanVorbisComment.hxx"
#include "XiphTags.hxx"
#include "tag/Table.hxx"
#include "tag/Handler.hxx"
#include "tag/VorbisComment.hxx"
#include "util/StringSplit.hxx"

/**
 * Check if the comment's name equals the passed name, and if so, copy
 * the comment value into the tag.
 */
static bool
vorbis_copy_comment(std::string_view comment,
		    std::string_view name, TagType tag_type,
		    TagHandler &handler) noexcept
{
	const auto value = GetVorbisCommentValue(comment, name);
	if (value.data() != nullptr) {
		handler.OnTag(tag_type, value);
		return true;
	}

	return false;
}

void
ScanVorbisComment(std::string_view comment, TagHandler &handler) noexcept
{
	if (handler.WantPair()) {
		const auto split = Split(comment, '=');
		if (!split.first.empty() && split.second.data() != nullptr)
			handler.OnPair(split.first, split.second);
	}

	for (const struct tag_table *i = xiph_tags; i->name != nullptr; ++i)
		if (vorbis_copy_comment(comment, i->name, i->type,
					handler))
			return;

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
		if (vorbis_copy_comment(comment,
					tag_item_names[i], TagType(i),
					handler))
			return;
}
