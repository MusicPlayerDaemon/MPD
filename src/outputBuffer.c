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

int sendDataToOutputBuffer(OutputBuffer * cb, DecoderControl * dc, 
                char * data, long datalen, float time, mpd_uint16 bitRate)
{
        mpd_uint16 dataToSend;
	mpd_uint16 chunkLeft;

        while(datalen) {
		if(currentChunk != cb->end) {
	        	while(cb->begin==cb->end && cb->wrap && !dc->stop && 
					!dc->seek)
			{
		        	my_usleep(10000);
			}
	        	if(dc->stop) return OUTPUT_BUFFER_DC_STOP;
	        	if(dc->seek) return OUTPUT_BUFFER_DC_SEEK;

			currentChunk = cb->end;
			cb->chunkSize[currentChunk] = 0;
		}

		chunkLeft = CHUNK_SIZE-cb->chunkSize[currentChunk];
                dataToSend = datalen > chunkLeft ? chunkLeft : datalen;

	        memcpy(cb->chunks+currentChunk*CHUNK_SIZE+
			cb->chunkSize[currentChunk],
			data, dataToSend);
	        cb->chunkSize[currentChunk]+= dataToSend;
	        cb->bitRate[currentChunk] = bitRate;
	        cb->times[currentChunk] = time;

                datalen-= dataToSend;
                data+= dataToSend;

		if(cb->chunkSize[currentChunk] == CHUNK_SIZE) {
			flushOutputBuffer(cb);
		}
        }

	return 0;
}
/* vim:set shiftwidth=4 tabstop=8 expandtab: */
