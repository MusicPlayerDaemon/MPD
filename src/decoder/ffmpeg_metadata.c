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
#include "tag_table.h"
#include "tag_handler.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ffmpeg"

static const struct tag_table ffmpeg_tags[] = {
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(52,50,0)
	{ "author", TAG_ARTIST },
#endif
	{ "year", TAG_DATE },
	{ "author-sort", TAG_ARTIST_SORT },
	{ "album_artist", TAG_ALBUM_ARTIST },
	{ "album_artist-sort", TAG_ALBUM_ARTIST_SORT },

	/* sentinel */
	{ NULL, TAG_NUM_OF_ITEM_TYPES }
};

static void
ffmpeg_copy_metadata(enum tag_type type,
		     AVDictionary *m, const char *name,
		     const struct tag_handler *handler, void *handler_ctx)
{
	AVDictionaryEntry *mt = NULL;

	while ((mt = av_dict_get(m, name, mt, 0)) != NULL)
		tag_handler_invoke_tag(handler, handler_ctx,
				       type, mt->value);
}

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(51,5,0)

static void
ffmpeg_scan_pairs(AVDictionary *dict,
		  const struct tag_handler *handler, void *handler_ctx)
{
	AVDictionaryEntry *i = NULL;

	while ((i = av_dict_get(dict, "", i, AV_DICT_IGNORE_SUFFIX)) != NULL)
		tag_handler_invoke_pair(handler, handler_ctx,
					i->key, i->value);
}

#endif

void
ffmpeg_scan_dictionary(AVDictionary *dict,
		       const struct tag_handler *handler, void *handler_ctx)
{
	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
		ffmpeg_copy_metadata(i, dict, tag_item_names[i],
				     handler, handler_ctx);

	for (const struct tag_table *i = ffmpeg_tags;
	     i->name != NULL; ++i)
		ffmpeg_copy_metadata(i->type, dict, i->name,
				     handler, handler_ctx);

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(51,5,0)
	if (handler->pair != NULL)
		ffmpeg_scan_pairs(dict, handler, handler_ctx);
#endif
}
