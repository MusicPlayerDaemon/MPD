/*
 * Copyright (C) 2003-2016 The Music Player Daemon Project
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
#include "Compiler.h"

class InputStream;
struct TagHandler;
struct Tag;
struct id3_tag;

#ifdef ENABLE_ID3TAG

bool
tag_id3_scan(InputStream &is,
	     const TagHandler &handler, void *handler_ctx);

Tag *
tag_id3_import(id3_tag *);

/**
 * Import all tags from the provided id3_tag *tag
 *
 */
void
scan_id3_tag(id3_tag *tag,
	     const TagHandler &handler, void *handler_ctx);

#else

static inline bool
tag_id3_scan(gcc_unused InputStream &is,
	     gcc_unused const TagHandler &handler,
	     gcc_unused void *handler_ctx)
{
	return false;
}

#endif

#endif
