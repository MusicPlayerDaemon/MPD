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
#include "log.h"
#include "utils.h"

#include <string.h>
#include <errno.h>

#define OUTPUT_BUFFER_DC_STOP   -1
#define OUTPUT_BUFFER_DC_SEEK   -2

long sendDataToOutputBuffer(Buffer * cb, DecoderControl * dc, int flushAllData,
                char * data, long datalen, float time, mpd_uint16 bitRate)
{
        long dataSent = 0;
        long dataToSend;

        while(datalen >= CHUNK_SIZE || (flushAllData && datalen)) {
                dataToSend = datalen > CHUNK_SIZE ? CHUNK_SIZE : datalen;

	        while(cb->begin==cb->end && cb->wrap && !dc->stop && !dc->seek) 
		        my_usleep(10000);
	        if(dc->stop) return OUTPUT_BUFFER_DC_STOP;
	        /* just for now, so it doesn't hang */
	        if(dc->seek) return OUTPUT_BUFFER_DC_SEEK;
	        /* be sure to remove this! */

	        memcpy(cb->chunks+cb->end*CHUNK_SIZE,data,dataToSend);
	        cb->chunkSize[cb->end] = dataToSend;
	        cb->bitRate[cb->end] = bitRate;
	        cb->times[cb->end] = time;

	        cb->end++;
	        if(cb->end>=buffered_chunks) {
		        cb->end = 0;
		        cb->wrap = 1;
	        }

                datalen-= dataToSend;
                dataSent+= dataToSend;
                data+= dataToSend;
        }

	return dataSent;
}
/* vim:set shiftwidth=4 tabstop=8 expandtab: */
