/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "VorbisComments.hxx"
#include "XiphTags.hxx"
#include "tag/Table.hxx"
#include "tag/Handler.hxx"
#include "tag/Builder.hxx"
#include "tag/Tag.hxx"
#include "tag/VorbisComment.hxx"
#include "tag/ReplayGain.hxx"
#include "ReplayGainInfo.hxx"
#include "util/StringView.hxx"

bool
vorbis_comments_to_replay_gain(ReplayGainInfo &rgi, char **comments) noexcept
{
	rgi.Clear();

	bool found = false;

	while (*comments) {
		if (ParseReplayGainVorbis(rgi, *comments))
			found = true;

		comments++;
	}

	return found;
}

/**
 * Check if the comment's name equals the passed name, and if so, copy
 * the comment value into the tag.
 */
static bool
vorbis_copy_comment(const char *comment,
		    const char *name, TagType tag_type,
		    TagHandler &handler) noexcept
{
	const char *value;

	value = vorbis_comment_value(comment, name);
	if (value != nullptr) {
		handler.OnTag(tag_type, value);
		return true;
	}

	return false;
}

static void
vorbis_scan_comment(const char *comment, TagHandler &handler) noexcept
{
	if (handler.WantPair()) {
		const auto split = StringView(comment).Split('=');
		if (!split.first.empty() && !split.second.IsNull())
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

void
vorbis_comments_scan(char **comments, TagHandler &handler) noexcept
{
	while (*comments)
		vorbis_scan_comment(*comments++, handler);

}

std::unique_ptr<Tag>
vorbis_comments_to_tag(char **comments) noexcept
{
	TagBuilder tag_builder;
	AddTagHandler h(tag_builder);
	vorbis_comments_scan(comments, h);
	return tag_builder.empty()
		? nullptr
		: tag_builder.CommitNew();
}
