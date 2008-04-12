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

#include "pcm_utils.h"
#include "playerData.h"
#include "utils.h"
#include "log.h"
#include "normalize.h"
#include "conf.h"
#include "os_compat.h"

static mpd_sint16 currentChunk = -1;

void initOutputBuffer(OutputBuffer * cb, char *chunks)
{
	memset(&cb->convState, 0, sizeof(ConvState));
	cb->chunks = chunks;
	cb->chunkSize = (mpd_uint16 *) (((char *)cb->chunks) +
					    buffered_chunks * CHUNK_SIZE);
	cb->bitRate = (mpd_uint16 *) (((char *)cb->chunkSize) +
					  buffered_chunks * sizeof(mpd_sint16));
	cb->times = (float *)(((char *)cb->bitRate) +
	             buffered_chunks * sizeof(mpd_sint8));
}

void clearOutputBuffer(OutputBuffer * cb)
{
	cb->end = cb->begin;
	currentChunk = -1;
}

void flushOutputBuffer(OutputBuffer * cb)
{
	if (currentChunk == cb->end) {
		if (((unsigned)cb->end + 1) >= buffered_chunks) {
			cb->end = 0;
		}
		else cb->end++;
		currentChunk = -1;
	}
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
			convBuffer = xrealloc(convBuffer, datalen);
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
		if (currentChunk != cb->end) {
			unsigned int next = cb->end + 1;
			if (next >= buffered_chunks) {
				next = 0;
			}
			while (cb->begin == next && !dc->stop) {
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
					decoder_sleep();
				}
			}
			if (dc->stop)
				return OUTPUT_BUFFER_DC_STOP;

			currentChunk = cb->end;
			cb->chunkSize[currentChunk] = 0;
			cb->bitRate[currentChunk] = bitRate;
			cb->times[currentChunk] = data_time;
		}

		chunkLeft = CHUNK_SIZE - cb->chunkSize[currentChunk];
		dataToSend = datalen > chunkLeft ? chunkLeft : datalen;

		memcpy(cb->chunks + currentChunk * CHUNK_SIZE +
		       cb->chunkSize[currentChunk], data, dataToSend);
		cb->chunkSize[currentChunk] += dataToSend;
		datalen -= dataToSend;
		data += dataToSend;

		if (cb->chunkSize[currentChunk] == CHUNK_SIZE) {
			flushOutputBuffer(cb);
		}
	}
	decoder_wakeup_player();

	return 0;
}

