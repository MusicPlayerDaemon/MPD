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
#include "audio.h"
#include "inputStream.h"
#include "replayGain.h"

#define OUTPUT_BUFFER_DC_STOP   -1
#define OUTPUT_BUFFER_DC_SEEK   -2

/* pick 1020 since its devisible for 8,16,24, and 32-bit audio */
#define CHUNK_SIZE		1020

typedef struct _OutputBufferChunk {
	volatile mpd_uint16 chunkSize;
	volatile mpd_uint16 bitRate;
	volatile float times;
	char data[CHUNK_SIZE];
} OutputBufferChunk;

/**
 * A ring set of buffers where the decoder appends data after the end,
 * and the player consumes data from the beginning.
 */
typedef struct _OutputBuffer {
	OutputBufferChunk *chunks;

	/** the index of the first decoded chunk */
	mpd_uint16 volatile begin;

	/** the index after the last decoded chunk */
	mpd_uint16 volatile end;

	mpd_sint16 currentChunk;

	AudioFormat audioFormat;
	ConvState convState;
} OutputBuffer;

void initOutputBuffer(OutputBuffer * cb);

void clearOutputBuffer(OutputBuffer * cb);

void flushOutputBuffer(OutputBuffer * cb);

/** is the buffer empty? */
int outputBufferEmpty(const OutputBuffer * cb);

void outputBufferShift(OutputBuffer * cb);

/**
 * what is the position of the specified chunk number, relative to
 * the first chunk in use?
 */
unsigned int outputBufferRelative(const OutputBuffer * cb, unsigned i);

/** determine the number of decoded chunks */
unsigned availableOutputBuffer(const OutputBuffer * cb);

/**
 * Get the absolute index of the nth used chunk after the first one.
 * Returns -1 if there is no such chunk.
 */
int outputBufferAbsolute(const OutputBuffer * cb, unsigned relative);

OutputBufferChunk * outputBufferGetChunk(const OutputBuffer * cb, unsigned i);

/* we send inStream for buffering the inputStream while waiting to
   send the next chunk */
int sendDataToOutputBuffer(OutputBuffer * cb,
			   InputStream * inStream,
			   DecoderControl * dc,
			   int seekable,
			   void *data,
			   size_t datalen,
			   float data_time,
			   mpd_uint16 bitRate, ReplayGainInfo * replayGainInfo);

#endif
