/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
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

#include <string.h>

static mpd_sint16 currentChunk = -1;

static mpd_sint8 currentMetaChunk = -1;
static mpd_sint8 sendMetaChunk = 0;

void clearAllMetaChunkSets(OutputBuffer * cb) {
	int i;

	for(i=0; i<BUFFERED_METACHUNKS; i++) {
		cb->metaChunkSet[i] = 0;
	}
}

void clearOutputBuffer(OutputBuffer * cb) {
        currentChunk = -1;
        cb->end = cb->begin;
        cb->wrap = 0;

	if(cb->acceptMetadata) {
		clearAllMetaChunkSets(cb);
		if(sendMetaChunk == 0 && currentMetaChunk >= 0) {
			cb->metaChunkSet[currentChunk] = 1;
		}
	}
}

void flushOutputBuffer(OutputBuffer * cb) {
	if(currentChunk == cb->end) {
	        cb->end++;
	        if(cb->end>=buffered_chunks) {
		       	cb->end = 0;
		       	cb->wrap = 1;
	        }
		currentChunk = -1;
	}
}

int sendDataToOutputBuffer(OutputBuffer * cb, InputStream * inStream,  
                DecoderControl * dc, int seekable, char * dataIn, 
                long dataInLen, float time, mpd_uint16 bitRate)
{
        mpd_uint16 dataToSend;
	mpd_uint16 chunkLeft;
	char * data;
	size_t datalen;
	static char * convBuffer = NULL;
	static long convBufferLen = 0;

	if(memcmp(&(cb->audioFormat),&(dc->audioFormat),sizeof(AudioFormat))==0)
	{
		data = dataIn;
		datalen = dataInLen;
	}
	else {
		datalen = pcm_sizeOfOutputBufferForAudioFormatConversion(
				&(dc->audioFormat), dataIn, dataInLen,
				&(cb->audioFormat));
		if(datalen > convBufferLen) {
			convBuffer = realloc(convBuffer,datalen);
			convBufferLen = datalen;
		}
		data = convBuffer;
		pcm_convertAudioFormat(&(dc->audioFormat), dataIn, dataInLen,
				&(cb->audioFormat),data);
	}

        while(datalen) {
		if(currentChunk != cb->end) {
	        	while(cb->begin==cb->end && cb->wrap && !dc->stop)
			{
                                if(dc->seek) {
                                        if(seekable) {
                                                return OUTPUT_BUFFER_DC_SEEK;
                                        }
                                        else {
                                                dc->seekError = 1;
                                                dc->seek = 0;
                                        }
                                }
                                if(!inStream || 
                                        bufferInputStream(inStream) <= 0)
                                {
		        	        my_usleep(10000);
                                }
			}
	        	if(dc->stop) return OUTPUT_BUFFER_DC_STOP;

			currentChunk = cb->end;
			cb->chunkSize[currentChunk] = 0;

			if(sendMetaChunk) {
				cb->metaChunk[currentChunk] = currentMetaChunk;
			}
			else cb->metaChunk[currentChunk] = -1;
	        	cb->bitRate[currentChunk] = bitRate;
	        	cb->times[currentChunk] = time;
		}

		chunkLeft = CHUNK_SIZE-cb->chunkSize[currentChunk];
                dataToSend = datalen > chunkLeft ? chunkLeft : datalen;

	        memcpy(cb->chunks+currentChunk*CHUNK_SIZE+
			cb->chunkSize[currentChunk],
			data, dataToSend);
	        cb->chunkSize[currentChunk]+= dataToSend;
                datalen-= dataToSend;
                data+= dataToSend;

		if(cb->chunkSize[currentChunk] == CHUNK_SIZE) {
			flushOutputBuffer(cb);
		}
        }

	return 0;
}

int copyMpdTagToOutputBuffer(OutputBuffer * cb, MpdTag * tag) {
	int nextChunk;

	printf("copyMpdTagToOB called\n");

        if(!cb->acceptMetadata || !tag) {
		sendMetaChunk = 0;
		return 0;
	}

	sendMetaChunk = 1;
	nextChunk = currentMetaChunk+1;
	if(nextChunk >= BUFFERED_METACHUNKS) nextChunk = 0;

	if(cb->metaChunkSet[nextChunk]) return -1;

	currentMetaChunk = nextChunk;

	printMpdTag(stdout, tag);

	copyMpdTagToMetadataChunk(tag, &(cb->metadataChunks[currentMetaChunk]));

	cb->metaChunkSet[nextChunk] = 1;

	return 0;
}
