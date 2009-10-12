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

#ifndef MPD_TEXT_INPUT_STREAM_H
#define MPD_TEXT_INPUT_STREAM_H

struct input_stream;
struct text_input_stream;

/**
 * Wraps an existing #input_stream object into a #text_input_stream,
 * to read its contents as text lines.
 *
 * @param is an open #input_stream object
 * @return the new #text_input_stream object
 */
struct text_input_stream *
text_input_stream_new(struct input_stream *is);

/**
 * Frees the #text_input_stream object.  Does not close or free the
 * underlying #input_stream.
 */
void
text_input_stream_free(struct text_input_stream *tis);

/**
 * Reads the next line from the stream.
 *
 * @return a line (newline character stripped), or NULL on end of file
 * or error
 */
const char *
text_input_stream_read(struct text_input_stream *tis);

#endif
