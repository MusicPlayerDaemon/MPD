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

#include "mp4_decode.h"

#ifdef HAVE_FAAD

#include "command.h"
#include "utils.h"
#include "audio.h"
#include "log.h"
#include "pcm_utils.h"

#include "mp4ff/mp4ff.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <faad.h>

/* all code here is either based on or copied from FAAD2's frontend code */

int mp4_getAACTrack(mp4ff_t *infile) {
	/* find AAC track */
	int i, rc;
	int numTracks = mp4ff_total_tracks(infile);

	for (i = 0; i < numTracks; i++) {
		unsigned char *buff = NULL;
		int buff_size = 0;
		mp4AudioSpecificConfig mp4ASC;
	
		mp4ff_get_decoder_config(infile, i, &buff, &buff_size);

		if (buff) {
			rc = AudioSpecificConfig(buff, buff_size, &mp4ASC);
			free(buff);
			if (rc < 0) continue;
            		return i;
		}
	}

	/* can't decode this */
	return -1;
}

uint32_t mp4_readCallback(void *user_data, void *buffer, uint32_t length) {
	return fread(buffer, 1, length, (FILE*)user_data);
}
            
uint32_t mp4_seekCallback(void *user_data, uint64_t position) {
	return fseek((FILE*)user_data, position, SEEK_SET);
}       
		    

int mp4_decode(Buffer * cb, AudioFormat * af, DecoderControl * dc) {
	FILE * fh;
	mp4ff_t * mp4fh;
	mp4ff_callback_t * mp4cb; 
	int32_t track;
	float time;
	int32_t scale;
	faacDecHandle decoder;
	faacDecFrameInfo frameInfo;
	faacDecConfigurationPtr config;
	unsigned char * mp4Buffer;
	int mp4BufferSize;
	unsigned long sampleRate;
	unsigned char channels;
	long sampleId;
	long numSamples;
	int eof = 0;
	long dur;
	unsigned int sampleCount;
	char * sampleBuffer;
	size_t sampleBufferLen;
	unsigned int initial = 1;
	int chunkLen = 0;
	float * seekTable;
	long seekTableEnd = -1;
	int seekPositionFound = 0;
	long offset;

	fh = fopen(dc->file,"r");
	if(!fh) {
		ERROR("failed to open %s\n",dc->file);
		return -1;
	}

	mp4cb = malloc(sizeof(mp4ff_callback_t));
	mp4cb->read = mp4_readCallback;
	mp4cb->seek = mp4_seekCallback;
	mp4cb->user_data = fh;

	mp4fh = mp4ff_open_read(mp4cb);
	if(!mp4fh) {
		ERROR("Input does not appear to be a mp4 stream.\n");
		free(mp4cb);
		fclose(fh);
		return -1;
	}

	track = mp4_getAACTrack(mp4fh);
	if(track < 0) {
		ERROR("No AAC track found in mp4 stream.\n");
		mp4ff_close(mp4fh);
		fclose(fh);
		free(mp4cb);
		return -1;
	}

	decoder = faacDecOpen();

	config = faacDecGetCurrentConfiguration(decoder);
	config->outputFormat = FAAD_FMT_16BIT;
	config->downMatrix = 1;
	config->dontUpSampleImplicitSBR = 1;
	faacDecSetConfiguration(decoder,config);

	af->bits = 16;

	mp4Buffer = NULL;
	mp4BufferSize = 0;
	mp4ff_get_decoder_config(mp4fh,track,&mp4Buffer,&mp4BufferSize);

	if(faacDecInit2(decoder,mp4Buffer,mp4BufferSize,&sampleRate,&channels)
			< 0)
	{
		ERROR("Error initializing AAC decoder library.\n");
		faacDecClose(decoder);
		mp4ff_close(mp4fh);
		free(mp4cb);
		fclose(fh);
		return -1;
	}

	af->sampleRate = sampleRate;
	af->channels = channels;
	time = mp4ff_get_track_duration_use_offsets(mp4fh,track);
	scale = mp4ff_time_scale(mp4fh,track);

	if(mp4Buffer) free(mp4Buffer);

	if(scale < 0) {
		ERROR("Error getting audio format of mp4 AAC track.\n");
		faacDecClose(decoder);
		mp4ff_close(mp4fh);
		fclose(fh);
		free(mp4cb);
		return -1;
	}
	cb->totalTime = ((float)time)/scale;

	numSamples = mp4ff_num_samples(mp4fh,track);

	dc->state = DECODE_STATE_DECODE;
	dc->start = 0;
	time = 0.0;

	seekTable = malloc(sizeof(float)*numSamples);

	for(sampleId=0; sampleId<numSamples && !eof; sampleId++) {
		if(dc->seek && seekTableEnd>1 && 
				seekTable[seekTableEnd]>=dc->seekWhere)
		{
			int i = 2;
			while(seekTable[i]<dc->seekWhere) i++;
			sampleId = i-1;
			time = seekTable[sampleId];
		}

		dur = mp4ff_get_sample_duration(mp4fh,track,sampleId);
		offset = mp4ff_get_sample_offset(mp4fh,track,sampleId);

		if(sampleId>seekTableEnd) {
			seekTable[sampleId] = time;
			seekTableEnd = sampleId;
		}

		if(sampleId==0) dur = 0;
		if(offset>dur) dur = 0;
		else dur-=offset;
		time+=((float)dur)/scale;

		if(dc->seek && time>dc->seekWhere) seekPositionFound = 1;

		if(dc->seek && seekPositionFound) {
			seekPositionFound = 0;
			chunkLen = 0;
			cb->end = 0;
			cb->wrap = 0;
			dc->seek = 0;
		}

		if(dc->seek) continue;
		
		if(mp4ff_read_sample(mp4fh,track,sampleId,&mp4Buffer,
				&mp4BufferSize) == 0)
		{
			eof = 1;
			continue;
		}

		sampleBuffer = faacDecDecode(decoder,&frameInfo,mp4Buffer,
						mp4BufferSize);
		if(mp4Buffer) free(mp4Buffer);
		if(frameInfo.error > 0) {
			eof = 1;
			break;
		}

		if(dur+offset > frameInfo.samples) {
			dur = frameInfo.samples;
			offset = 0;
		}
			
		sampleCount = (unsigned long)(dur*channels);

		if(sampleCount>0) initial =0;
		sampleBufferLen = sampleCount*2;

		sampleBuffer+=offset*channels*2;

		while(sampleBufferLen>0 && !dc->seek) {
			size_t size = sampleBufferLen>CHUNK_SIZE-chunkLen ? 
							CHUNK_SIZE-chunkLen:
							sampleBufferLen;
#ifdef WORDS_BIGENDIAN
			pcm_changeBufferEndianness(sampleBuffer,size,af->bits);
#endif
			while(cb->begin==cb->end && cb->wrap &&
					!dc->stop && !dc->seek)
			{
					usleep(10000);
			}
			if(dc->stop) {
				eof = 1;
				break;
			}
			else if(!dc->seek) {
				sampleBufferLen-=size;
				memcpy(cb->chunks+cb->end*CHUNK_SIZE+chunkLen,
						sampleBuffer,size);
				cb->times[cb->end] = time;
				sampleBuffer+=size;
				chunkLen+=size;
				if(chunkLen>=CHUNK_SIZE) {
					cb->chunkSize[cb->end] = CHUNK_SIZE;
					++cb->end;
		
					if(cb->end>=buffered_chunks) {
						cb->end = 0;
						cb->wrap = 1;
					}
					chunkLen = 0;
				}
			}
		}
	}

	if(!dc->stop && !dc->seek && chunkLen>0) {
		cb->chunkSize[cb->end] = chunkLen;
		++cb->end;
	
		if(cb->end>=buffered_chunks) {
			cb->end = 0;
			cb->wrap = 1;
		}
		chunkLen = 0;
	}

	free(seekTable);
	faacDecClose(decoder);
	mp4ff_close(mp4fh);
	fclose(fh);
	free(mp4cb);

	if(dc->seek) dc->seek = 0;

	if(dc->stop) {
		dc->state = DECODE_STATE_STOP;
		dc->stop = 0;
	}
	else dc->state = DECODE_STATE_STOP;

	return 0;
}

#endif /* HAVE_FAAD */
