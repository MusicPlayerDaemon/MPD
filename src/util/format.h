/*
 * music player command (mpc)
 * Copyright 2003-2021 The Music Player Daemon Project
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

#ifndef MPC_FORMAT_H
#define MPC_FORMAT_H

#include "Compiler.h"

struct mpd_song;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Pretty-print an object into a string using the given format
 * specification.
 *
 * @param format the format string
 * @param object the object
 * @param getter a getter function that extracts a value from the object
 * @return the resulting string to be freed by free(); NULL if
 * no format string group produced any output
 */
gcc_malloc
char *
format_object(const char *format, const void *object,
	      const char *(*getter)(const void *object, const char *name));

#ifdef __cplusplus
}
#endif

#endif
