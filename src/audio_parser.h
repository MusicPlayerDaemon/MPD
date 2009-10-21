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

/** \file
 *
 * Parser functions for audio related objects.
 */

#ifndef AUDIO_PARSER_H
#define AUDIO_PARSER_H

#include <glib.h>

#include <stdbool.h>

struct audio_format;

/**
 * Parses a string in the form "SAMPLE_RATE:BITS:CHANNELS" into an
 * #audio_format.
 *
 * @param dest the destination #audio_format struct
 * @param src the input string
 * @param mask if true, then "*" is allowed for any number of items
 * @param error_r location to store the error occuring, or NULL to
 * ignore errors
 * @return true on success
 */
bool
audio_format_parse(struct audio_format *dest, const char *src,
		   bool mask, GError **error_r);

#endif
