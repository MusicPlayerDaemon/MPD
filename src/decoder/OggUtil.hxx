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

#ifndef MPD_OGG_UTIL_HXX
#define MPD_OGG_UTIL_HXX

#include "check.h"

#include <ogg/ogg.h>

#include <stddef.h>

/**
 * Feed data from the #input_stream into the #ogg_sync_state.
 *
 * @return false on error or end-of-file
 */
bool
OggFeed(ogg_sync_state &oy, struct decoder *decoder,
	struct input_stream *input_stream, size_t size);

/**
 * Feed into the #ogg_sync_state until a page gets available.  Garbage
 * data at the beginning is considered a fatal error.
 *
 * @return true if a page is available
 */
bool
OggExpectPage(ogg_sync_state &oy, ogg_page &page,
	      struct decoder *decoder, struct input_stream *input_stream);

#endif
