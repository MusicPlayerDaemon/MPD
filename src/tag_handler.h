/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef MPD_TAG_HANDLER_H
#define MPD_TAG_HANDLER_H

#include "check.h"
#include "tag.h"

#include <assert.h>

/**
 * A callback table for receiving metadata of a song.
 */
struct tag_handler {
	/**
	 * Declare the duration of a song, in seconds.  Do not call
	 * this when the duration could not be determined, because
	 * there is no magic value for "unknown duration".
	 */
	void (*duration)(unsigned seconds, void *ctx);

	/**
	 * A tag has been read.
	 *
	 * @param the value of the tag; the pointer will become
	 * invalid after returning
	 */
	void (*tag)(enum tag_type type, const char *value, void *ctx);

	/**
	 * A name-value pair has been read.  It is the codec specific
	 * representation of tags.
	 */
	void (*pair)(const char *key, const char *value, void *ctx);
};

static inline void
tag_handler_invoke_duration(const struct tag_handler *handler, void *ctx,
			  unsigned seconds)
{
	assert(handler != NULL);

	if (handler->duration != NULL)
		handler->duration(seconds, ctx);
}

static inline void
tag_handler_invoke_tag(const struct tag_handler *handler, void *ctx,
		       enum tag_type type, const char *value)
{
	assert(handler != NULL);
	assert((unsigned)type < TAG_NUM_OF_ITEM_TYPES);
	assert(value != NULL);

	if (handler->tag != NULL)
		handler->tag(type, value, ctx);
}

static inline void
tag_handler_invoke_pair(const struct tag_handler *handler, void *ctx,
			const char *name, const char *value)
{
	assert(handler != NULL);
	assert(name != NULL);
	assert(value != NULL);

	if (handler->pair != NULL)
		handler->pair(name, value, ctx);
}

/**
 * This #tag_handler implementation adds tag values to a #tag object
 * (casted from the context pointer).
 */
extern const struct tag_handler add_tag_handler;

/**
 * This #tag_handler implementation adds tag values to a #tag object
 * (casted from the context pointer), and supports the has_playlist
 * attribute.
 */
extern const struct tag_handler full_tag_handler;

#endif
