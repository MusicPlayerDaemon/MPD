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
#include "ffmpeg_metadata.h"
#include "tag.h"
#include "tag_table.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ffmpeg"

static const struct tag_table ffmpeg_tags[] = {
#if LIBAVFORMAT_VERSION_INT < ((52<<16)+(50<<8))
	{ "author", TAG_ARTIST },
	{ "year", TAG_DATE },
#endif
	{ "author-sort", TAG_ARTIST_SORT },
	{ "album_artist", TAG_ALBUM_ARTIST },
	{ "album_artist-sort", TAG_ALBUM_ARTIST_SORT },

	/* sentinel */
	{ NULL, TAG_NUM_OF_ITEM_TYPES }
};

static void
ffmpeg_copy_metadata(struct tag *tag, enum tag_type type,
		     AVDictionary *m, const char *name)
{
	AVDictionaryEntry *mt = NULL;

	while ((mt = av_dict_get(m, name, mt, 0)) != NULL)
		tag_add_item(tag, type, mt->value);
}

void
ffmpeg_copy_dictionary(struct tag *tag, AVDictionary *dict)
{
	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
		ffmpeg_copy_metadata(tag, i,
				     dict, tag_item_names[i]);

	for (const struct tag_table *i = ffmpeg_tags;
	     i->name != NULL; ++i)
		ffmpeg_copy_metadata(tag, i->type, dict, i->name);
}
