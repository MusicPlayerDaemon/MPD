/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef MPD_PIPE_H
#define MPD_PIPE_H

#include <glib.h>
#include <stdbool.h>

#ifndef NDEBUG
struct audio_format;
#endif

struct music_chunk;
struct music_buffer;

/**
 * A queue of #music_chunk objects.  One party appends chunks at the
 * tail, and the other consumes them from the head.
 */
struct music_pipe;

/**
 * Creates a new #music_pipe object.  It is empty.
 */
G_GNUC_MALLOC
struct music_pipe *
music_pipe_new(void);

/**
 * Frees the object.  It must be empty now.
 */
void
music_pipe_free(struct music_pipe *mp);

#ifndef NDEBUG

/**
 * Checks if the audio format if the chunk is equal to the specified
 * audio_format.
 */
bool
music_pipe_check_format(const struct music_pipe *pipe,
			const struct audio_format *audio_format);

/**
 * Checks if the specified chunk is enqueued in the music pipe.
 */
bool
music_pipe_contains(const struct music_pipe *mp,
		    const struct music_chunk *chunk);

#endif

/**
 * Returns the first #music_chunk from the pipe.  Returns NULL if the
 * pipe is empty.
 */
G_GNUC_PURE
const struct music_chunk *
music_pipe_peek(const struct music_pipe *mp);

/**
 * Removes the first chunk from the head, and returns it.
 */
struct music_chunk *
music_pipe_shift(struct music_pipe *mp);

/**
 * Clears the whole pipe and returns the chunks to the buffer.
 *
 * @param buffer the buffer object to return the chunks to
 */
void
music_pipe_clear(struct music_pipe *mp, struct music_buffer *buffer);

/**
 * Pushes a chunk to the tail of the pipe.
 */
void
music_pipe_push(struct music_pipe *mp, struct music_chunk *chunk);

/**
 * Returns the number of chunks currently in this pipe.
 */
G_GNUC_PURE
unsigned
music_pipe_size(const struct music_pipe *mp);

G_GNUC_PURE
static inline bool
music_pipe_empty(const struct music_pipe *mp)
{
	return music_pipe_size(mp) == 0;
}

#endif
