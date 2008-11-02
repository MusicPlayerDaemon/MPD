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

#include "audio_format.h"

#include <stddef.h>
#include <stdbool.h>

/* pick 1020 since its devisible for 8,16,24, and 32-bit audio */
#define CHUNK_SIZE		1020

typedef struct _OutputBufferChunk {
	uint16_t chunkSize;
	uint16_t bitRate;
	float times;
	char data[CHUNK_SIZE];
} ob_chunk;

/**
 * A ring set of buffers where the decoder appends data after the end,
 * and the player consumes data from the beginning.
 */
struct output_buffer {
	ob_chunk *chunks;

	unsigned int size;

	/** the index of the first decoded chunk */
	unsigned int volatile begin;

	/** the index after the last decoded chunk */
	unsigned int volatile end;

	/** non-zero if the player thread should only we woken up if
	    the buffer becomes non-empty */
	bool lazy;

	struct audio_format audioFormat;

	struct notify *notify;
};

extern struct output_buffer ob;

void
ob_init(unsigned int size, struct notify *notify);

void ob_free(void);

void ob_clear(void);

void ob_flush(void);

/**
 * When a chunk is decoded, we wake up the player thread to tell him
 * about it.  In "lazy" mode, we only wake him up when the buffer was
 * previously empty, i.e. when the player thread has really been
 * waiting for us.
 */
void ob_set_lazy(bool lazy);

/** is the buffer empty? */
static inline bool ob_is_empty(void)
{
	return ob.begin == ob.end;
}

void ob_shift(void);

/**
 * what is the position of the specified chunk number, relative to
 * the first chunk in use?
 */
unsigned int ob_relative(const unsigned i);

/** determine the number of decoded chunks */
unsigned ob_available(void);

/**
 * Get the absolute index of the nth used chunk after the first one.
 * Returns -1 if there is no such chunk.
 */
int ob_absolute(const unsigned relative);

ob_chunk * ob_get_chunk(const unsigned i);

/**
 * Append a data block to the buffer.
 *
 * @return the number of bytes actually written
 */
size_t ob_append(const void *data, size_t datalen,
		 float data_time, uint16_t bitRate);

void ob_skip(unsigned num);

#endif
