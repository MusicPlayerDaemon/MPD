/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
 * This project's homepage is: http://www.musicpd.org
 * 
 * libaudiofile (wave) support added by Eric Wong <normalperson@yhbt.net>
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

#include "audiofile_decode.h"

#ifdef HAVE_AUDIOFILE

#include "command.h"
#include "utils.h"
#include "audio.h"
#include "log.h"
#include "pcm_utils.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <audiofile.h>

int getAudiofileTotalTime(char * file)
{
	int time;
	AFfilehandle af_fp = afOpenFile(file, "r", NULL);
	if(af_fp == AF_NULL_FILEHANDLE) {
		return -1;
	}
	time = (int)
		((double)afGetFrameCount(af_fp,AF_DEFAULT_TRACK)
		 /afGetRate(af_fp,AF_DEFAULT_TRACK));
	afCloseFile(af_fp);
	return time;
}

int audiofile_decode(Buffer * cb, AudioFormat * af, DecoderControl * dc)
{
	int fs, frame_count;
	AFfilehandle af_fp;
	int bits;
		
	af_fp = afOpenFile(dc->file,"r", NULL);
	if(af_fp == AF_NULL_FILEHANDLE) {
		ERROR("failed to open %s\n",dc->file);
		return -1;
	}

	afGetSampleFormat(af_fp, AF_DEFAULT_TRACK, &fs, &bits);
	af->bits = bits;
	af->sampleRate = afGetRate(af_fp, AF_DEFAULT_TRACK);
	af->channels = afGetChannels(af_fp,AF_DEFAULT_TRACK);
	
	frame_count = afGetFrameCount(af_fp,AF_DEFAULT_TRACK);
	
	cb->totalTime = ((float)frame_count/(float)af->sampleRate);
	
	if (af->bits != 8 && af->bits != 16) {
		ERROR("Only 8 and 16-bit files are supported. %s is %i-bit\n",
			dc->file,af->bits);
		afCloseFile(af_fp);
		return -1;
	}
	
	fs = (int)afGetFrameSize(af_fp, AF_DEFAULT_TRACK,1);

	dc->state = DECODE_STATE_DECODE;
	dc->start = 0;
	{
		int ret, eof = 0, current = 0;
		unsigned char chunk[CHUNK_SIZE];

		while(!eof) {
			if(dc->seek) {
				cb->end = 0;
				cb->wrap = 0;
				current = dc->seekWhere * af->sampleRate;
				afSeekFrame(af_fp, AF_DEFAULT_TRACK,current);
				
				dc->seek = 0;
			}

			ret = afReadFrames(af_fp, AF_DEFAULT_TRACK, chunk, CHUNK_SIZE/fs);
			if(ret<=0) eof = 1;
			else {
				while(cb->begin==cb->end && cb->wrap &&
						!dc->stop && !dc->seek){
					usleep(10000);
				}
				if(dc->stop) break;
				else if(dc->seek) continue;
				
#ifdef WORDS_BIGENDIAN
				pcm_changeBufferEndianness(chunk,CHUNK_SIZE,
						af->bits);
#endif
				memcpy(cb->chunks+cb->end*CHUNK_SIZE,chunk,
						CHUNK_SIZE);
				cb->chunkSize[cb->end] = CHUNK_SIZE;
				
				current += ret;
				cb->times[cb->end] = (float)current/(float)af->sampleRate;
				
				++cb->end;
				
				if(cb->end>=buffered_chunks) {
					cb->end = 0;
					cb->wrap = 1;
				}
			}
		}

		if(dc->seek) dc->seek = 0;

		if(dc->stop) {
			dc->state = DECODE_STATE_STOP;
			dc->stop = 0;
		}
		else dc->state = DECODE_STATE_STOP;
	}
	afCloseFile(af_fp);

	return 0;
}

#endif /* HAVE_AUDIOFILE */
