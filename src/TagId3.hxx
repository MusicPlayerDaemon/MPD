/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#ifndef MPD_TAG_ID3_HXX
#define MPD_TAG_ID3_HXX

#include "check.h"
#include "gcc.h"
#include "gerror.h"

struct tag_handler;
struct Tag;
struct id3_tag;

#ifdef HAVE_ID3TAG

bool
tag_id3_scan(const char *path_fs,
	     const struct tag_handler *handler, void *handler_ctx);

Tag *
tag_id3_import(struct id3_tag *);

/**
 * Loads the ID3 tags from the file into a libid3tag object.  The
 * return value must be freed with id3_tag_delete().
 *
 * @return NULL on error or if no ID3 tag was found in the file (no
 * GError will be set)
 */
struct id3_tag *
tag_id3_load(const char *path_fs, GError **error_r);

/**
 * Import all tags from the provided id3_tag *tag
 *
 */
void
scan_id3_tag(struct id3_tag *tag,
	     const struct tag_handler *handler, void *handler_ctx);

#else

static inline bool
tag_id3_scan(gcc_unused const char *path_fs,
	     gcc_unused const struct tag_handler *handler,
	     gcc_unused void *handler_ctx)
{
	return false;
}

#endif

#endif
