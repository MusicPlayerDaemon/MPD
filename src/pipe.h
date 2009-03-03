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

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct audio_format;
struct tag;
struct music_chunk;

/**
 * A ring set of buffers where the decoder appends data after the end,
 * and the player consumes data from the beginning.
 */
struct music_pipe {
	struct music_chunk *chunks;
	unsigned num_chunks;

	/** the index of the first decoded chunk */
	unsigned begin;

	/** the index after the last decoded chunk */
	unsigned end;

	/** non-zero if the player thread should only we woken up if
	    the buffer becomes non-empty */
	bool lazy;

	struct notify *notify;
};

extern struct music_pipe music_pipe;

void
music_pipe_init(unsigned int size, struct notify *notify);

void music_pipe_free(void);

void music_pipe_clear(void);

void music_pipe_flush(void);

/**
 * When a chunk is decoded, we wake up the player thread to tell him
 * about it.  In "lazy" mode, we only wake him up when the buffer was
 * previously empty, i.e. when the player thread has really been
 * waiting for us.
 */
void music_pipe_set_lazy(bool lazy);

static inline unsigned
music_pipe_size(void)
{
	return music_pipe.num_chunks;
}

/** is the buffer empty? */
static inline bool music_pipe_is_empty(void)
{
	return music_pipe.begin == music_pipe.end;
}

static inline bool
music_pipe_head_is(unsigned i)
{
	return !music_pipe_is_empty() && music_pipe.begin == i;
}

static inline unsigned
music_pipe_tail_index(void)
{
	return music_pipe.end;
}

void music_pipe_shift(void);

/**
 * what is the position of the specified chunk number, relative to
 * the first chunk in use?
 */
unsigned int music_pipe_relative(const unsigned i);

/** determine the number of decoded chunks */
unsigned music_pipe_available(void);

/**
 * Get the absolute index of the nth used chunk after the first one.
 * Returns -1 if there is no such chunk.
 */
int music_pipe_absolute(const unsigned relative);

struct music_chunk *
music_pipe_get_chunk(const unsigned i);

static inline struct music_chunk *
music_pipe_peek(void)
{
	if (music_pipe_is_empty())
		return NULL;

	return music_pipe_get_chunk(music_pipe.begin);
}

/**
 * Prepares appending to the music pipe.  Returns a buffer where you
 * may write into.  After you are finished, call music_pipe_expand().
 *
 * @return a writable buffer
 */
void *
music_pipe_write(const struct audio_format *audio_format,
		 float data_time, uint16_t bit_rate,
		 size_t *max_length_r);

/**
 * Tells the music pipe to move the end pointer, after you have
 * written to the buffer returned by music_pipe_write().
 */
void
music_pipe_expand(const struct audio_format *audio_format, size_t length);

/**
 * Send a tag.  This is usually called when a new song within a stream
 * begins.
 */
bool music_pipe_tag(const struct tag *tag);

void music_pipe_skip(unsigned num);

/**
 * Chop off the tail of the music pipe, starting with the chunk at
 * index "first".
 */
void music_pipe_chop(unsigned first);

#ifndef NDEBUG
void music_pipe_check_format(const struct audio_format *current,
			     int next_index, const struct audio_format *next);
#endif

#endif
