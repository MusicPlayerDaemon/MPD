// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ScanVorbisComment.hxx"
#include "XiphTags.hxx"
#include "tag/Names.hxx"
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
