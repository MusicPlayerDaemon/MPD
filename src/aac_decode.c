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

#include "aac_decode.h"

#ifdef HAVE_FAAD

#define AAC_MAX_CHANNELS	6

#include "command.h"
#include "utils.h"
#include "audio.h"
#include "log.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <faad.h>

/* all code here is either based on or copied from FAAD2's frontend code */
typedef struct {
	long bytesIntoBuffer;
	long bytesConsumed;
	long fileOffset;
	unsigned char *buffer;
	int atEof;
	FILE *infile;
} AacBuffer;

void fillAacBuffer(AacBuffer *b) {
	if(b->bytesConsumed > 0) {
		int bread;

		if(b->bytesIntoBuffer) {
			memmove((void *)b->buffer,(void*)(b->buffer+
					b->bytesConsumed),b->bytesIntoBuffer);
		}

		if(!b->atEof) {
			bread = fread((void *)(b->buffer+b->bytesIntoBuffer),1,
					b->bytesConsumed,b->infile);
			if(bread!=b->bytesConsumed) b->atEof = 1;
			b->bytesIntoBuffer+=bread;
		}

		b->bytesConsumed = 0;

		if(b->bytesIntoBuffer > 3) {
			if(memcmp(b->buffer,"TAG",3)==0) b->bytesIntoBuffer = 0;
		}
		if(b->bytesIntoBuffer > 11) {
			if(memcmp(b->buffer,"LYRICSBEGIN",11)==0) {
				b->bytesIntoBuffer = 0;
			}
		}
		if(b->bytesIntoBuffer > 8) {
			if(memcmp(b->buffer,"APETAGEX",8)==0) {
				b->bytesIntoBuffer = 0;
			}
		}
	}
}

void advanceAacBuffer(AacBuffer * b, int bytes) {
	b->fileOffset+=bytes;
	b->bytesConsumed = bytes;
	b->bytesIntoBuffer-=bytes;
}

static int adtsSampleRates[] = {96000,88200,64000,48000,44100,32000,24000,22050,
				16000,12000,11025,8000,7350,0,0,0};

int adtsParse(AacBuffer * b, float * length) {
	int frames, frameLength;
	int tFrameLength = 0;
	int sampleRate = 0;
	float framesPerSec, bytesPerFrame;

	/* Read all frames to ensure correct time and bitrate */
	for(frames = 0; ;frames++) {
		fillAacBuffer(b);

		if(b->bytesIntoBuffer > 7) {
			/* check syncword */
			if (!((b->buffer[0] == 0xFF) && 
				((b->buffer[1] & 0xF6) == 0xF0)))
			{
				break;
			}

			if(frames==0) {
				sampleRate = adtsSampleRates[
						(b->buffer[2]&0x3c)>>2];
			}

			frameLength =  ((((unsigned int)b->buffer[3] & 0x3)) 
					<< 11) | (((unsigned int)b->buffer[4])  
                			<< 3) | (b->buffer[5] >> 5);

			tFrameLength+=frameLength;

			if(frameLength > b->bytesIntoBuffer) break;

			advanceAacBuffer(b,frameLength);
		}
		else break;
	}

	framesPerSec = (float)sampleRate/1024.0;
	if(frames!=0) {
		bytesPerFrame = (float)tFrameLength/(float)(frames*1000);
	}
	else bytesPerFrame = 0;
	if(framesPerSec!=0) *length = (float)frames/framesPerSec;

	return 1;
}

int initAacBuffer(char * file, AacBuffer * b, float * length) {
	size_t fileread;
	size_t bread;
	size_t tagsize;

	*length = -1;

	memset(b,0,sizeof(AacBuffer));

	b->infile = fopen(file,"r");
	if(b->infile == NULL) return -1;

	fseek(b->infile,0,SEEK_END);
	fileread = ftell(b->infile);
	fseek(b->infile,0,SEEK_SET);

	b->buffer = malloc(FAAD_MIN_STREAMSIZE*AAC_MAX_CHANNELS);
	memset(b->buffer,0,FAAD_MIN_STREAMSIZE*AAC_MAX_CHANNELS);

	bread = fread(b->buffer,1,FAAD_MIN_STREAMSIZE*AAC_MAX_CHANNELS,
			b->infile);
	b->bytesIntoBuffer = bread;
	b->bytesConsumed = 0;
	b->fileOffset = 0;

	if(bread!=FAAD_MIN_STREAMSIZE*AAC_MAX_CHANNELS) b->atEof = 1;

	tagsize = 0;
	if(!memcmp(b->buffer,"ID3",3)) {
		tagsize = (b->buffer[6] << 21) | (b->buffer[7] << 14) |
				(b->buffer[8] << 7) | (b->buffer[9] << 0);

		tagsize+=10;
		advanceAacBuffer(b,tagsize);
		fillAacBuffer(b);
	}

	if((b->buffer[0] == 0xFF) && ((b->buffer[1] & 0xF6) == 0xF0)) {
		adtsParse(b, length);
		fseek(b->infile, tagsize, SEEK_SET);

		bread = fread(b->buffer, 1, 
				FAAD_MIN_STREAMSIZE*AAC_MAX_CHANNELS,
				b->infile);
		if(bread != FAAD_MIN_STREAMSIZE*AAC_MAX_CHANNELS) b->atEof = 1;
		else b->atEof = 0;
		b->bytesIntoBuffer = bread;
		b->bytesConsumed = 0;
		b->fileOffset = tagsize;
	}
	else if(memcmp(b->buffer,"ADIF",4) == 0) {
		int bitRate;
		int skipSize = (b->buffer[4] & 0x80) ? 9 : 0;
		bitRate = ((unsigned int)(b->buffer[4 + skipSize] & 0x0F)<<19) |
            			((unsigned int)b->buffer[5 + skipSize]<<11) |
            			((unsigned int)b->buffer[6 + skipSize]<<3) |
            			((unsigned int)b->buffer[7 + skipSize] & 0xE0);

		*length = fileread;
		if(*length!=0 && bitRate!=0) *length = *length*8.0/bitRate;
	}

	if(*length<0) return -1;

	return 0;
}

int getAacTotalTime(char * file) {
	AacBuffer b;
	float length;

	if(initAacBuffer(file,&b,&length) < 0) return -1;

	if(b.buffer) free(b.buffer);
	fclose(b.infile);

	return (int)(length+0.5);
}


int aac_decode(Buffer * cb, AudioFormat * af, DecoderControl * dc) {
	/*FILE * fh;
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
	mpd_uint16 bitRate = 0;

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
#ifdef HAVE_FAACDECCONFIGURATION_DOWNMATRIX
	config->downMatrix = 1;
#endif
#ifdef HAVE_FAACDECCONFIGURATION_DONTUPSAMPLEIMPLICITSBR
	config->dontUpSampleImplicitSBR = 0;
#endif
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

		if(channels*(dur+offset) > frameInfo.samples) {
			dur = frameInfo.samples;
			offset = 0;
		}

		sampleCount = (unsigned long)(dur*channels);

		if(sampleCount>0) {
			initial =0;
			bitRate = frameInfo.bytesconsumed*8.0*
				frameInfo.channels*scale/
				frameInfo.samples/1024+0.5;
		}
			

		sampleBufferLen = sampleCount*2;

		sampleBuffer+=offset*channels*2;

		while(sampleBufferLen>0 && !dc->seek) {
			size_t size = sampleBufferLen>CHUNK_SIZE-chunkLen ? 
							CHUNK_SIZE-chunkLen:
							sampleBufferLen;
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
				cb->bitRate[cb->end] = bitRate;
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
	else dc->state = DECODE_STATE_STOP;*/

	return 0;
}

#endif /* HAVE_FAAD */
