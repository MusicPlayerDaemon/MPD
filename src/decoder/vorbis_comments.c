/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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
#include "vorbis_comments.h"
#include "tag.h"
#include "tag_table.h"
#include "tag_handler.h"
#include "replay_gain_info.h"

#include <glib.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

static const char *
vorbis_comment_value(const char *comment, const char *needle)
{
	size_t len = strlen(needle);

	if (g_ascii_strncasecmp(comment, needle, len) == 0 &&
	    comment[len] == '=')
		return comment + len + 1;

	return NULL;
}

bool
vorbis_comments_to_replay_gain(struct replay_gain_info *rgi, char **comments)
{
	const char *temp;
	bool found = false;

	replay_gain_info_init(rgi);

	while (*comments) {
		if ((temp =
		     vorbis_comment_value(*comments, "replaygain_track_gain"))) {
			rgi->tuples[REPLAY_GAIN_TRACK].gain = atof(temp);
			found = true;
		} else if ((temp = vorbis_comment_value(*comments,
							"replaygain_album_gain"))) {
			rgi->tuples[REPLAY_GAIN_ALBUM].gain = atof(temp);
			found = true;
		} else if ((temp = vorbis_comment_value(*comments,
							"replaygain_track_peak"))) {
			rgi->tuples[REPLAY_GAIN_TRACK].peak = atof(temp);
			found = true;
		} else if ((temp = vorbis_comment_value(*comments,
							"replaygain_album_peak"))) {
			rgi->tuples[REPLAY_GAIN_ALBUM].peak = atof(temp);
			found = true;
		}

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
		    const char *name, enum tag_type tag_type,
		    const struct tag_handler *handler, void *handler_ctx)
{
	const char *value;

	value = vorbis_comment_value(comment, name);
	if (value != NULL) {
		tag_handler_invoke_tag(handler, handler_ctx, tag_type, value);
		return true;
	}

	return false;
}

static const struct tag_table vorbis_tags[] = {
	{ "tracknumber", TAG_TRACK },
	{ "discnumber", TAG_DISC },
	{ "album artist", TAG_ALBUM_ARTIST },
	{ NULL, TAG_NUM_OF_ITEM_TYPES }
};

static void
vorbis_scan_comment(const char *comment,
		    const struct tag_handler *handler, void *handler_ctx)
{
	if (handler->pair != NULL) {
		char *name = g_strdup((const char*)comment);
		char *value = strchr(name, '=');

		if (value != NULL && value > name) {
			*value++ = 0;
			tag_handler_invoke_pair(handler, handler_ctx,
						name, value);
		}

		g_free(name);
	}

	for (const struct tag_table *i = vorbis_tags; i->name != NULL; ++i)
		if (vorbis_copy_comment(comment, i->name, i->type,
					handler, handler_ctx))
			return;

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
		if (vorbis_copy_comment(comment,
					tag_item_names[i], i,
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

struct tag *
vorbis_comments_to_tag(char **comments)
{
	struct tag *tag = tag_new();
	vorbis_comments_scan(comments, &add_tag_handler, tag);

	if (tag_is_empty(tag)) {
		tag_free(tag);
		tag = NULL;
	}

	return tag;
}
