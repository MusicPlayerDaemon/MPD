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

#ifndef MPD_TEXT_FILE_H
#define MPD_TEXT_FILE_H

#include <glib.h>

#include <stdio.h>

/**
 * Reads a line from the input file, and strips trailing space.  There
 * is a reasonable maximum line length, only to prevent denial of
 * service.
 *
 * @param file the source file, opened in text mode
 * @param buffer an allocator for the buffer
 * @return a pointer to the line, or NULL on end-of-file or error
 */
char *
read_text_line(FILE *file, GString *buffer);

#endif
