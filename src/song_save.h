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

#ifndef MPD_SONG_SAVE_H
#define MPD_SONG_SAVE_H

#include <glib.h>

#include <stdbool.h>
#include <stdio.h>

#define SONG_BEGIN "song_begin: "

struct songvec;
struct directory;

void songvec_save(FILE *fp, struct songvec *sv);

/**
 * Loads a song from the input file.  Reading stops after the
 * "song_end" line.
 *
 * @param error_r location to store the error occuring, or NULL to
 * ignore errors
 * @return true on success, false on error
 */
struct song *
song_load(FILE *fp, struct directory *parent, const char *uri,
	  GString *buffer, GError **error_r);

#endif
