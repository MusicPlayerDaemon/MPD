/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#ifndef MPD_TAG_ID3_H
#define MPD_TAG_ID3_H

#include "check.h"

struct tag;

#ifdef HAVE_ID3TAG
struct id3_tag;
struct tag *tag_id3_import(struct id3_tag *);

struct tag *tag_id3_load(const char *file);

#else

#include <glib.h>

static inline struct tag *
tag_id3_load(G_GNUC_UNUSED const char *file)
{
	return NULL;
}

#endif

#endif
