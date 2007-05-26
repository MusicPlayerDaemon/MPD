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

#include <string.h>

static mpd_sint16 currentChunk = -1;

static mpd_sint8 currentMetaChunk = -1;
static mpd_sint8 sendMetaChunk;

void clearAllMetaChunkSets(OutputBuffer * cb)
{
	memset(cb->metaChunkSet, 0, BUFFERED_METACHUNKS);
}

void clearOutputBuffer(OutputBuffer * cb)
{
	int currentSet = 1;

	cb->end = cb->begin;

	/* be sure to reset metaChunkSets cause we are skipping over audio
	 * audio chunks, and thus skipping over metadata */
	if (currentChunk >= 0 && sendMetaChunk == 0 && currentMetaChunk >= 0) {
		currentSet = cb->metaChunkSet[currentChunk];
	}
	clearAllMetaChunkSets(cb);
	if (currentChunk >= 0 && sendMetaChunk == 0 && currentMetaChunk >= 0) {
		cb->metaChunkSet[currentChunk] = currentSet;
	}
	currentChunk = -1;
}

void flushOutputBuffer(OutputBuffer * cb)
{
	if (currentChunk == cb->end) {
		if ((cb->end + 1) >= buffered_chunks) {
			cb->end = 0;
		}
		else cb->end++;
		currentChunk = -1;
	}
}

int sendDataToOutputBuffer(OutputBuffer * cb, InputStream * inStream,
			   DecoderControl * dc, int seekable, void *dataIn,
			   long dataInLen, float time, mpd_uint16 bitRate,
			   ReplayGainInfo * replayGainInfo)
{
	mpd_uint16 dataToSend;
	mpd_uint16 chunkLeft;
	char *data;
	size_t datalen;
	static char *convBuffer;
	static long convBufferLen;

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
			int next = cb->end + 1;
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
					}
				}
				if (!inStream ||
				    bufferInputStream(inStream) <= 0) {
					my_usleep(10000);
				}
			}
			if (dc->stop)
				return OUTPUT_BUFFER_DC_STOP;

			currentChunk = cb->end;
			cb->chunkSize[currentChunk] = 0;

			if (sendMetaChunk) {
				cb->metaChunk[currentChunk] = currentMetaChunk;
			} else
				cb->metaChunk[currentChunk] = -1;
			cb->bitRate[currentChunk] = bitRate;
			cb->times[currentChunk] = time;
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

	return 0;
}

int copyMpdTagToOutputBuffer(OutputBuffer * cb, MpdTag * tag)
{
	int nextChunk;
	static MpdTag *last;

	if (!cb->acceptMetadata || !tag) {
		sendMetaChunk = 0;
		if (last)
			freeMpdTag(last);
		last = NULL;
		DEBUG("copyMpdTagToOB: !acceptMetadata || !tag\n");
		return 0;
	}

	if (last && mpdTagsAreEqual(last, tag)) {
		DEBUG("copyMpdTagToOB: same as last\n");
		return 0;
	}

	if (last)
		freeMpdTag(last);
	last = NULL;

	nextChunk = currentMetaChunk + 1;
	if (nextChunk >= BUFFERED_METACHUNKS)
		nextChunk = 0;

	if (cb->metaChunkSet[nextChunk]) {
		sendMetaChunk = 0;
		DEBUG("copyMpdTagToOB: metachunk in use!\n");
		return -1;
	}

	sendMetaChunk = 1;
	currentMetaChunk = nextChunk;

	last = mpdTagDup(tag);

	copyMpdTagToMetadataChunk(tag, &(cb->metadataChunks[currentMetaChunk]));

	cb->metaChunkSet[nextChunk] = 1;

	DEBUG("copyMpdTagToOB: copiedTag\n");

	return 0;
}
