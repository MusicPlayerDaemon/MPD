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

#ifndef ICY_METADATA_H
#define ICY_METADATA_H

#include <stdbool.h>
#include <stddef.h>

struct icy_metadata {
	size_t data_size, data_rest;

	size_t meta_size, meta_position;
	char *meta_data;

	struct tag *tag;
};

/**
 * Initialize a disabled icy_metadata object.
 */
static inline void
icy_clear(struct icy_metadata *im)
{
	im->data_size = 0;
}

/**
 * Initialize an enabled icy_metadata object with the specified
 * data_size (from the icy-metaint HTTP response header).
 */
static inline void
icy_start(struct icy_metadata *im, size_t data_size)
{
	im->data_size = im->data_rest = data_size;
	im->meta_size = 0;
	im->tag = NULL;
}

/**
 * Resets the icy_metadata.  Call this after rewinding the stream.
 */
void
icy_reset(struct icy_metadata *im);

void
icy_deinit(struct icy_metadata *im);

/**
 * Checks whether the icy_metadata object is enabled.
 */
static inline bool
icy_defined(const struct icy_metadata *im)
{
	return im->data_size > 0;
}

/**
 * Evaluates data.  Returns the number of bytes of normal data which
 * can be read by the caller, but not more than "length".  If the
 * return value is smaller than "length", the caller should invoke
 * icy_meta().
 */
size_t
icy_data(struct icy_metadata *im, size_t length);

/**
 * Reads metadata from the stream.  Returns the number of bytes
 * consumed.  If the return value is smaller than "length", the caller
 * should invoke icy_data().
 */
size_t
icy_meta(struct icy_metadata *im, const void *data, size_t length);

static inline struct tag *
icy_tag(struct icy_metadata *im)
{
	struct tag *tag = im->tag;
	im->tag = NULL;
	return tag;
}

#endif
