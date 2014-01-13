/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

/** \file
 *
 * A parser for the RIFF file format (e.g. WAV).
 */

#ifndef MPD_RIFF_HXX
#define MPD_RIFF_HXX

#include <stddef.h>
#include <stdio.h>

/**
 * Seeks the RIFF file to the ID3 chunk.
 *
 * @return the size of the ID3 chunk on success, or 0 if this is not a
 * RIFF file or no ID3 chunk was found
 */
size_t
riff_seek_id3(FILE *file);

#endif
