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

#ifdef HAVE_OGG
#include "ogg_decode.h"

#include "command.h"
#include "utils.h"
#include "audio.h"
#include "log.h"
#include "pcm_utils.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <vorbis/vorbisfile.h>

int ogg_decode(Buffer * cb, AudioFormat * af, DecoderControl * dc)
{
	OggVorbis_File vf;
	FILE * oggfp;
	
	if(!(oggfp = fopen(dc->file,"r"))) {
		ERROR("failed to open ogg\n");
		return -1;
	}
		
	if(ov_open(oggfp, &vf, NULL, 0) < 0) {
		ERROR("Input does not appear to be an Ogg bit stream.\n");
		fclose(oggfp);
		return -1;
	}
	
	{
		vorbis_info *vi=ov_info(&vf,-1);
		af->bits = 16;
		af->channels = vi->channels;
		af->sampleRate = vi->rate;
	}

	cb->totalTime = ov_time_total(&vf,-1);
	dc->state = DECODE_STATE_DECODE;
	dc->start = 0;

	{
		int current_section;
		int eof = 0;
		long ret;
		char chunk[CHUNK_SIZE];
		int chunkpos = 0;
		long bitRate = 0;
		long test;

		while(!eof) {
			if(dc->seek) {
				cb->end = 0;
				cb->wrap = 0;
				chunkpos = 0;
				ov_time_seek_page(&vf,dc->seekWhere);
				dc->seek = 0;
			}
			ret = ov_read(&vf,chunk+chunkpos,
					CHUNK_SIZE-chunkpos,
					0,2,1,
					&current_section);
			if(ret<=0) eof = 1;
			else chunkpos+=ret;
			if(chunkpos>=CHUNK_SIZE || eof) {
				while(cb->begin==cb->end && cb->wrap &&
						!dc->stop && !dc->seek)
				{
					usleep(10000);
				}
				if(dc->stop) break;
				else if(dc->seek) continue;
#ifdef WORDS_BIGENDIAN
				pcm_changeBufferEndianness(chunk,CHUNK_SIZE,
						af->bits);
#endif
				memcpy(cb->chunks+cb->end*CHUNK_SIZE,
						chunk,chunkpos);
				cb->chunkSize[cb->end] = chunkpos;
				chunkpos = 0;
				cb->times[cb->end] = ov_time_tell(&vf);
				if((test = ov_bitrate_instant(&vf))>0) {
					bitRate = test/1000;
				}
				cb->bitRate[cb->end] = bitRate;
				cb->end++;
				if(cb->end>=buffered_chunks) {
					cb->end = 0;
					cb->wrap = 1;
				}
			}
		}

		ov_clear(&vf);

		if(dc->seek) dc->seek = 0;

		if(dc->stop) {
			dc->state = DECODE_STATE_STOP;
			dc->stop = 0;
		}
		else dc->state = DECODE_STATE_STOP;
	}

	return 0;
}

#endif
