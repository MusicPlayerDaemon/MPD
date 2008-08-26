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
#include "playerData.h"

void ob_init(unsigned int size)
{
	assert(size > 0);

	memset(&ob.convState, 0, sizeof(ConvState));
	ob.chunks = xmalloc(size * sizeof(*ob.chunks));
	ob.size = size;
	ob.begin = 0;
	ob.end = 0;
	ob.lazy = 0;
	ob.chunks[0].chunkSize = 0;
}

void ob_free(void)
{
	assert(ob.chunks != NULL);
	free(ob.chunks);
}

void ob_clear(void)
{
	ob.end = ob.begin;
	ob.chunks[ob.end].chunkSize = 0;
}

/** return the index of the chunk after i */
static inline unsigned successor(unsigned i)
{
	assert(i <= ob.size);

	++i;
	return i == ob.size ? 0 : i;
}

/**
 * Mark the tail chunk as "full" and wake up the player if is waiting
 * for the decoder.
 */
static void output_buffer_expand(unsigned i)
{
	int was_empty = !ob.lazy || ob_is_empty();

	assert(i == (ob.end + 1) % ob.size);
	assert(i != ob.end);

	ob.end = i;
	ob.chunks[i].chunkSize = 0;
	if (was_empty)
		/* if the buffer was empty, the player thread might be
		   waiting for us; wake it up now that another decoded
		   buffer has become available. */
		decoder_wakeup_player();
}

void ob_flush(void)
{
	ob_chunk *chunk = ob_get_chunk(ob.end);

	if (chunk->chunkSize > 0) {
		unsigned int next = successor(ob.end);
		if (next == ob.begin)
			/* all buffers are full; we have to wait for
			   the player to free one, so don't flush
			   right now */
			return;

		output_buffer_expand(next);
	}
}

void ob_set_lazy(int lazy)
{
	ob.lazy = lazy;
}

int ob_is_empty(void)
{
	return ob.begin == ob.end;
}

void ob_shift(void)
{
	assert(ob.begin != ob.end);
	assert(ob.begin < ob.size);

	ob.begin = successor(ob.begin);
}

unsigned int ob_relative(const unsigned i)
{
	if (i >= ob.begin)
		return i - ob.begin;
	else
		return i + ob.size - ob.begin;
}

unsigned ob_available(void)
{
	return ob_relative(ob.end);
}

int ob_absolute(const unsigned relative)
{
	unsigned i, max;

	max = ob.end;
	if (max < ob.begin)
		max += ob.size;
	i = (unsigned)ob.begin + relative;
	if (i >= max)
		return -1;

	if (i >= ob.size)
		i -= ob.size;

	return (int)i;
}

ob_chunk * ob_get_chunk(const unsigned i)
{
	assert(i < ob.size);

	return &ob.chunks[i];
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
static int tailChunk(InputStream * inStream,
		     int seekable, float data_time, mpd_uint16 bitRate)
{
	unsigned int next;
	ob_chunk *chunk;

	chunk = ob_get_chunk(ob.end);
	assert(chunk->chunkSize <= sizeof(chunk->data));
	if (chunk->chunkSize == sizeof(chunk->data)) {
		/* this chunk is full; allocate a new chunk */
		next = successor(ob.end);
		while (ob.begin == next) {
			/* all chunks are full of decoded data; wait
			   for the player to free one */

			if (dc.command == DECODE_COMMAND_STOP)
				return OUTPUT_BUFFER_DC_STOP;

			if (dc.command == DECODE_COMMAND_SEEK) {
				if (seekable) {
					return OUTPUT_BUFFER_DC_SEEK;
				} else {
					dc.seekError = 1;
					dc_command_finished();
				}
			}
			if (!inStream || bufferInputStream(inStream) <= 0) {
				decoder_sleep();
			}
		}

		output_buffer_expand(next);
		chunk = ob_get_chunk(next);
		assert(chunk->chunkSize == 0);
	}

	if (chunk->chunkSize == 0) {
		/* if the chunk is empty, nobody has set bitRate and
		   times yet */

		chunk->bitRate = bitRate;
		chunk->times = data_time;
	}

	return ob.end;
}

int ob_send(InputStream * inStream,
			   int seekable, void *dataIn,
			   size_t dataInLen, float data_time, mpd_uint16 bitRate,
			   ReplayGainInfo * replayGainInfo)
{
	size_t dataToSend;
	char *data;
	size_t datalen;
	static char *convBuffer;
	static size_t convBufferLen;
	ob_chunk *chunk = NULL;

	if (cmpAudioFormat(&(ob.audioFormat), &(dc.audioFormat)) == 0) {
		data = dataIn;
		datalen = dataInLen;
	} else {
		datalen = pcm_sizeOfConvBuffer(&(dc.audioFormat), dataInLen,
		                               &(ob.audioFormat));
		if (datalen > convBufferLen) {
			if (convBuffer != NULL)
				free(convBuffer);
			convBuffer = xmalloc(datalen);
			convBufferLen = datalen;
		}
		data = convBuffer;
		datalen = pcm_convertAudioFormat(&(dc.audioFormat), dataIn,
		                                 dataInLen, &(ob.audioFormat),
		                                 data, &(ob.convState));
	}

	if (replayGainInfo && (replayGainState != REPLAYGAIN_OFF))
		doReplayGain(replayGainInfo, data, datalen, &ob.audioFormat);
	else if (normalizationEnabled)
		normalizeData(data, datalen, &ob.audioFormat);

	while (datalen) {
		int chunk_index = tailChunk(inStream, seekable,
					    data_time, bitRate);
		if (chunk_index < 0)
			return chunk_index;

		chunk = ob_get_chunk(chunk_index);

		dataToSend = sizeof(chunk->data) - chunk->chunkSize;
		if (dataToSend > datalen)
			dataToSend = datalen;

		memcpy(chunk->data + chunk->chunkSize, data, dataToSend);
		chunk->chunkSize += dataToSend;
		datalen -= dataToSend;
		data += dataToSend;
	}

	if (chunk != NULL && chunk->chunkSize == sizeof(chunk->data))
		ob_flush();

	return 0;
}

void ob_skip(unsigned num)
{
	int i = ob_absolute(num);
	if (i >= 0)
		ob.begin = i;
}
