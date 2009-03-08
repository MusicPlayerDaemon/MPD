/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MPD_PIPE_H
#define MPD_PIPE_H

#ifndef NDEBUG
#include <stdbool.h>

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
#endif

/**
 * Returns the first #music_chunk from the pipe.  Returns NULL if the
 * pipe is empty.
 */
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
unsigned
music_pipe_size(const struct music_pipe *mp);

#endif
