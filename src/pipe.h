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

/* pick 1020 since its devisible for 8,16,24, and 32-bit audio */
#define CHUNK_SIZE		1020

struct audio_format;

struct music_chunk {
	uint16_t length;
	uint16_t bit_rate;
	float times;
	char data[CHUNK_SIZE];
};

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
 * Append a data block to the buffer.
 *
 * @return the number of bytes actually written
 */
size_t music_pipe_append(const void *data, size_t datalen,
			 const struct audio_format *audio_format,
			 float data_time, uint16_t bit_rate);

void music_pipe_skip(unsigned num);

#endif
