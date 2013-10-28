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

#ifndef MPD_DECODER_DSDLIB_HXX
#define MPD_DECODER_DSDLIB_HXX

#include "Compiler.h"

#include <stdlib.h>
#include <stdint.h>

struct Decoder;
struct InputStream;

struct DsdId {
	char value[4];

	gcc_pure
	bool Equals(const char *s) const;
};

bool
dsdlib_read(Decoder *decoder, InputStream &is,
	    void *data, size_t length);

bool
dsdlib_skip_to(Decoder *decoder, InputStream &is,
	       int64_t offset);

bool
dsdlib_skip(Decoder *decoder, InputStream &is,
	    int64_t delta);

/**
 * Add tags from ID3 tag. All tags commonly found in the ID3 tags of
 * DSF and DSDIFF files are imported
 */
void
dsdlib_tag_id3(InputStream &is,
	       const struct tag_handler *handler,
	       void *handler_ctx, int64_t tagoffset);

#endif
