/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "VorbisComments.hxx"
#include "XiphTags.hxx"
#include "tag/TagTable.hxx"
#include "tag/TagHandler.hxx"
#include "tag/TagBuilder.hxx"
#include "tag/VorbisComment.hxx"
#include "tag/ReplayGain.hxx"
#include "ReplayGainInfo.hxx"
#include "util/ASCII.hxx"
#include "util/SplitString.hxx"

#include <stddef.h>
#include <stdlib.h>

bool
vorbis_comments_to_replay_gain(ReplayGainInfo &rgi, char **comments)
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
		    const struct tag_handler *handler, void *handler_ctx)
{
	const char *value;

	value = vorbis_comment_value(comment, name);
	if (value != nullptr) {
		tag_handler_invoke_tag(handler, handler_ctx, tag_type, value);
		return true;
	}

	return false;
}

static void
vorbis_scan_comment(const char *comment,
		    const struct tag_handler *handler, void *handler_ctx)
{
	if (handler->pair != nullptr) {
		const SplitString split(comment, '=');
		if (split.IsDefined() && !split.IsEmpty())
			tag_handler_invoke_pair(handler, handler_ctx,
						split.GetFirst(),
						split.GetSecond());
	}

	for (const struct tag_table *i = xiph_tags; i->name != nullptr; ++i)
		if (vorbis_copy_comment(comment, i->name, i->type,
					handler, handler_ctx))
			return;

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
		if (vorbis_copy_comment(comment,
					tag_item_names[i], TagType(i),
					handler, handler_ctx))
			return;
}

void
vorbis_comments_scan(char **comments,
		     const struct tag_handler *handler, void *handler_ctx)
{
	while (*comments)
		vorbis_scan_comment(*comments++,
				    handler, handler_ctx);

}

Tag *
vorbis_comments_to_tag(char **comments)
{
	TagBuilder tag_builder;
	vorbis_comments_scan(comments, &add_tag_handler, &tag_builder);
	return tag_builder.IsEmpty()
		? nullptr
		: tag_builder.CommitNew();
}
