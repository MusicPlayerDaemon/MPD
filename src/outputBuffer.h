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

#ifndef OUTPUT_BUFFER_H
#define OUTPUT_BUFFER_H

#include "pcm_utils.h"
#include "mpd_types.h"
#include "decode.h"
#include "inputStream.h"
#include "replayGain.h"

#define OUTPUT_BUFFER_DC_STOP   -1
#define OUTPUT_BUFFER_DC_SEEK   -2

/* pick 1020 since its devisible for 8,16,24, and 32-bit audio */
#define CHUNK_SIZE		1020

typedef struct _OutputBufferChunk {
	mpd_uint16 chunkSize;
	mpd_uint16 bitRate;
	float times;
	char data[CHUNK_SIZE];
} ob_chunk;

/**
 * A ring set of buffers where the decoder appends data after the end,
 * and the player consumes data from the beginning.
 */
typedef struct _OutputBuffer {
	ob_chunk *chunks;

	unsigned int size;

	/** the index of the first decoded chunk */
	unsigned int volatile begin;

	/** the index after the last decoded chunk */
	unsigned int volatile end;

	AudioFormat audioFormat;
	ConvState convState;
} OutputBuffer;

void ob_init(unsigned int size);

void ob_free(void);

void ob_clear(void);

void ob_flush(void);

/** is the buffer empty? */
int ob_is_empty(void);

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

/* we send inStream for buffering the inputStream while waiting to
   send the next chunk */
int ob_send(InputStream * inStream,
			   int seekable,
			   void *data,
			   size_t datalen,
			   float data_time,
			   mpd_uint16 bitRate, ReplayGainInfo * replayGainInfo);

void ob_skip(unsigned num);

#endif
