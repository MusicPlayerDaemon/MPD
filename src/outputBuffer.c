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

#include "outputBuffer.h"

#include "utils.h"
#include "normalize.h"

void initOutputBuffer(OutputBuffer * cb, unsigned int size)
{
	assert(size > 0);

	memset(&cb->convState, 0, sizeof(ConvState));
	cb->chunks = xmalloc(size * sizeof(*cb->chunks));
	cb->size = size;
	cb->begin = 0;
	cb->end = 0;
	cb->chunks[0].chunkSize = 0;
}

void output_buffer_free(OutputBuffer * cb)
{
	assert(cb->chunks != NULL);
	free(cb->chunks);
}

void clearOutputBuffer(OutputBuffer * cb)
{
	cb->end = cb->begin;
	cb->chunks[cb->end].chunkSize = 0;
}

/** return the index of the chunk after i */
static inline unsigned successor(const OutputBuffer * cb, unsigned i)
{
	assert(i <= cb->size);

	++i;
	return i == cb->size ? 0 : i;
}

/**
 * Mark the tail chunk as "full" and wake up the player if is waiting
 * for the decoder.
 */
static void output_buffer_expand(OutputBuffer * cb, unsigned i)
{
	int was_empty = outputBufferEmpty(cb);

	assert(i == (cb->end + 1) % cb->size);
	assert(i != cb->end);

	cb->end = i;
	cb->chunks[i].chunkSize = 0;
	if (was_empty)
		/* if the buffer was empty, the player thread might be
		   waiting for us; wake it up now that another decoded
		   buffer has become available. */
		decoder_wakeup_player();
}

void flushOutputBuffer(OutputBuffer * cb)
{
	OutputBufferChunk *chunk = outputBufferGetChunk(cb, cb->end);

	if (chunk->chunkSize > 0) {
		unsigned int next = successor(cb, cb->end);
		if (next == cb->begin)
			/* all buffers are full; we have to wait for
			   the player to free one, so don't flush
			   right now */
			return;

		output_buffer_expand(cb, next);
	}
}

int outputBufferEmpty(const OutputBuffer * cb)
{
	return cb->begin == cb->end;
}

void outputBufferShift(OutputBuffer * cb)
{
	assert(cb->begin != cb->end);
	assert(cb->begin < cb->size);

	cb->begin = successor(cb, cb->begin);
}

unsigned int outputBufferRelative(const OutputBuffer * cb, unsigned i)
{
	if (i >= cb->begin)
		return i - cb->begin;
	else
		return i + cb->size - cb->begin;
}

unsigned availableOutputBuffer(const OutputBuffer * cb)
{
	return outputBufferRelative(cb, cb->end);
}

int outputBufferAbsolute(const OutputBuffer * cb, unsigned relative)
{
	unsigned i, max;

	max = cb->end;
	if (max < cb->begin)
		max += cb->size;
	i = (unsigned)cb->begin + relative;
	if (i >= max)
		return -1;

	if (i >= cb->size)
		i -= cb->size;

	return (int)i;
}

OutputBufferChunk * outputBufferGetChunk(const OutputBuffer * cb, unsigned i)
{
	assert(i < cb->size);

	return &cb->chunks[i];
}

/**
 * Return the tail chunk which has room for additional data.  If there
 * is no room in the queue, this function blocks until the player
 * thread has finished playing its current chunk.
 *
 * @return the positive index of the new chunk; OUTPUT_BUFFER_DC_SEEK
 * if another thread requested seeking; OUTPUT_BUFFER_DC_STOP if
 * another thread requested stopping the decoder.
 */
static int tailChunk(OutputBuffer * cb, InputStream * inStream,
		     DecoderControl * dc, int seekable,
		     float data_time, mpd_uint16 bitRate)
{
	unsigned int next;
	OutputBufferChunk *chunk;

	chunk = outputBufferGetChunk(cb, cb->end);
	assert(chunk->chunkSize <= sizeof(chunk->data));
	if (chunk->chunkSize == sizeof(chunk->data)) {
		/* this chunk is full; allocate a new chunk */
		next = successor(cb, cb->end);
		while (cb->begin == next) {
			/* all chunks are full of decoded data; wait
			   for the player to free one */

			if (dc->stop)
				return OUTPUT_BUFFER_DC_STOP;

			if (dc->seek) {
				if (seekable) {
					return OUTPUT_BUFFER_DC_SEEK;
				} else {
					dc->seekError = 1;
					dc->seek = 0;
					decoder_wakeup_player();
				}
			}
			if (!inStream ||
			    bufferInputStream(inStream) <= 0) {
				decoder_sleep(dc);
			}
		}

		output_buffer_expand(cb, next);
		chunk = outputBufferGetChunk(cb, next);
		assert(chunk->chunkSize == 0);
	}

	if (chunk->chunkSize == 0) {
		/* if the chunk is empty, nobody has set bitRate and
		   times yet */

		chunk->bitRate = bitRate;
		chunk->times = data_time;
	}

	return cb->end;
}

int sendDataToOutputBuffer(OutputBuffer * cb, InputStream * inStream,
			   DecoderControl * dc, int seekable, void *dataIn,
			   size_t dataInLen, float data_time, mpd_uint16 bitRate,
			   ReplayGainInfo * replayGainInfo)
{
	mpd_uint16 dataToSend;
	mpd_uint16 chunkLeft;
	char *data;
	size_t datalen;
	static char *convBuffer;
	static size_t convBufferLen;

	if (cmpAudioFormat(&(cb->audioFormat), &(dc->audioFormat)) == 0) {
		data = dataIn;
		datalen = dataInLen;
	} else {
		datalen = pcm_sizeOfConvBuffer(&(dc->audioFormat), dataInLen,
		                               &(cb->audioFormat));
		if (datalen > convBufferLen) {
			if (convBuffer != NULL)
				free(convBuffer);
			convBuffer = xmalloc(datalen);
			convBufferLen = datalen;
		}
		data = convBuffer;
		datalen = pcm_convertAudioFormat(&(dc->audioFormat), dataIn,
		                                 dataInLen, &(cb->audioFormat),
		                                 data, &(cb->convState));
	}

	if (replayGainInfo && (replayGainState != REPLAYGAIN_OFF))
		doReplayGain(replayGainInfo, data, datalen, &cb->audioFormat);
	else if (normalizationEnabled)
		normalizeData(data, datalen, &cb->audioFormat);

	while (datalen) {
		OutputBufferChunk *chunk;
		int chunk_index = tailChunk(cb, inStream,
					    dc, seekable,
					    data_time, bitRate);
		if (chunk_index < 0)
			return chunk_index;

		chunk = outputBufferGetChunk(cb, chunk_index);

		chunkLeft = sizeof(chunk->data) - chunk->chunkSize;
		dataToSend = datalen > chunkLeft ? chunkLeft : datalen;

		memcpy(chunk->data + chunk->chunkSize, data, dataToSend);
		chunk->chunkSize += dataToSend;
		datalen -= dataToSend;
		data += dataToSend;

		if (chunk->chunkSize == sizeof(chunk->data)) {
			flushOutputBuffer(cb);
		}
	}

	return 0;
}

void output_buffer_skip(OutputBuffer * cb, unsigned num)
{
	int i = outputBufferAbsolute(cb, num);
	if (i >= 0)
		cb->begin = i;
}
