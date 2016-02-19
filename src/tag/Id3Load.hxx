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

#ifndef MPD_TAG_ID3_LOAD_HXX
#define MPD_TAG_ID3_LOAD_HXX

#include "check.h"

struct id3_tag;
class Path;
class Error;

/**
 * Loads the ID3 tags from the file into a libid3tag object.  The
 * return value must be freed with id3_tag_delete().
 *
 * @return nullptr on error or if no ID3 tag was found in the file (no
 * Error will be set)
 */
struct id3_tag *
tag_id3_load(Path path_fs, Error &error);

#endif
