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

#ifdef HAVE_FLAC
#include "flac_decode.h"

#include "utils.h"
#include "log.h"
#include "pcm_utils.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <FLAC/file_decoder.h>
#include <FLAC/metadata.h>

typedef struct {
	unsigned char chunk[CHUNK_SIZE];
	int chunk_length;
	float time;
	int bitRate;
	FLAC__uint64 position;
	Buffer * cb;
	AudioFormat * af;
	DecoderControl * dc;
	char * file;
} FlacData;

/* this code is based on flac123, from flac-tools */

int flacSendChunk(FlacData * data);
void flacPlayfile(const char * file, Buffer * cb, ao_sample_format * format);
void flacError(const FLAC__FileDecoder *, FLAC__StreamDecoderErrorStatus, void *);
void flacPrintErroredState(FLAC__FileDecoderState state, char * file);
void flacMetadata(const FLAC__FileDecoder *, const FLAC__StreamMetadata *, void *);
FLAC__StreamDecoderWriteStatus flacWrite(const FLAC__FileDecoder *, const FLAC__Frame *, const FLAC__int32 * const buf[], void *);

void flacPlayFile(char *file, Buffer * cb, AudioFormat * af, 
	DecoderControl *dc)
{
	FLAC__FileDecoder * flacDec;
	FlacData data;
	int status = 1;

	data.chunk_length = 0;
	data.time = 0;
	data.position = 0;
	data.bitRate = 0;
	data.cb = cb;
	data.af = af;
	data.dc = dc;
	data.file = file;
	
	if(!(flacDec = FLAC__file_decoder_new())) return;
	/*status&=FLAC__file_decoder_set_md5_checking(flacDec,1);*/
	status&=FLAC__file_decoder_set_filename(flacDec,file);
	status&=FLAC__file_decoder_set_write_callback(flacDec,flacWrite);
	status&=FLAC__file_decoder_set_metadata_callback(flacDec,flacMetadata);
	status&=FLAC__file_decoder_set_error_callback(flacDec,flacError);
	status&=FLAC__file_decoder_set_client_data(flacDec, (void *)&data);
	if(!status) {
		ERROR("flac problem before init(): %s\n",file);
		flacPrintErroredState(FLAC__file_decoder_get_state(flacDec),file);
		FLAC__file_decoder_delete(flacDec);
		return;
	}
	if(FLAC__file_decoder_init(flacDec)!=
			FLAC__STREAM_DECODER_SEARCH_FOR_METADATA) 
	{
		ERROR("flac problem doing init(): %s\n",file);
		flacPrintErroredState(FLAC__file_decoder_get_state(flacDec),file);
		FLAC__file_decoder_delete(flacDec);
		return;
	}
	if(!FLAC__file_decoder_process_until_end_of_metadata(flacDec)) {
		ERROR("flac problem reading metadata: %s\n",file);
		flacPrintErroredState(FLAC__file_decoder_get_state(flacDec),file);
		FLAC__file_decoder_delete(flacDec);
		return;
	}
	while(1) {
		FLAC__file_decoder_process_single(flacDec);
		if(FLAC__file_decoder_get_state(flacDec)!=
				FLAC__FILE_DECODER_OK)
		{
			break;
		}
		if(dc->seek) {
			FLAC__uint64 sampleToSeek = dc->seekWhere*
					af->sampleRate+0.5;
			cb->end = 0;
			cb->wrap = 0;
			if(FLAC__file_decoder_seek_absolute(flacDec,
						sampleToSeek))
			{
				data.time = ((float)sampleToSeek)/
					af->sampleRate;
				data.position = 0;
			}
			dc->seek = 0;
		}
	}
	FLAC__file_decoder_process_until_end_of_file(flacDec);
	if(!dc->stop) {
		flacPrintErroredState(FLAC__file_decoder_get_state(flacDec),
				file);
		FLAC__file_decoder_finish(flacDec);
	}
	FLAC__file_decoder_delete(flacDec);
	/* send last little bit */
	if(data.chunk_length>0 && !dc->stop) flacSendChunk(&data);
}

void flacError(const FLAC__FileDecoder *dec, FLAC__StreamDecoderErrorStatus status, void *fdata) {
	FlacData * data = (FlacData *) fdata;
	if(data->dc->stop) return;

	switch(status) {
	case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
		ERROR("flac lost sync: %s\n",data->file);
		break;
	case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
		ERROR("bad header %s\n",data->file);
		break;
	case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
		ERROR("crc mismatch %s\n",data->file);
		break;
	default:
		ERROR("unknow flac error %s\n",data->file);
	}
}

void flacPrintErroredState(FLAC__FileDecoderState state, char * file) {
	switch(state) {
	case FLAC__FILE_DECODER_ERROR_OPENING_FILE:
		ERROR("error opening flac: %s\n",file);
		break;
	case FLAC__FILE_DECODER_MEMORY_ALLOCATION_ERROR:
		ERROR("flac allocation error\n");
		break;
	case FLAC__FILE_DECODER_SEEK_ERROR:
		ERROR("flac seek error: %s\n",file);
		break;
	case FLAC__FILE_DECODER_SEEKABLE_STREAM_DECODER_ERROR:
		ERROR("flac seekable stream error: %s\n",file);
		break;
	case FLAC__FILE_DECODER_ALREADY_INITIALIZED:
		ERROR("flac decoder already initilaized: %s\n",file);
		break;
	case FLAC__FILE_DECODER_INVALID_CALLBACK:
		ERROR("invalid flac callback\n");
		break;
	case FLAC__FILE_DECODER_UNINITIALIZED:
		ERROR("flac decoder uninitialized: %s\n",file);
		break;
	case FLAC__FILE_DECODER_OK:
	case FLAC__FILE_DECODER_END_OF_FILE:
		break;
	}
}

void flacMetadata(const FLAC__FileDecoder *dec, const FLAC__StreamMetadata *meta, void *data) {
}

int flacSendChunk(FlacData * data) {
	while(data->cb->begin==data->cb->end && data->cb->wrap && 
		!data->dc->stop && !data->dc->seek)  
	{
		usleep(1000);
	}

	if(data->dc->stop) return -1;
	if(data->dc->seek) return 0;

#ifdef WORDS_BIGENDIAN
	pcm_changeBufferEndianness(chunk,CHUNK_SIZE,data->af->bits);
#endif
	memcpy(data->cb->chunks+data->cb->end*CHUNK_SIZE,data->chunk,
			CHUNK_SIZE);
	data->cb->chunkSize[data->cb->end] = data->chunk_length;
	data->cb->times[data->cb->end] = data->time;
	data->cb->bitRate[data->cb->end] = data->bitRate;

	data->cb->end++;
	if(data->cb->end>=buffered_chunks) {
		data->cb->end = 0;
		data->cb->wrap = 1;
	}

	return 0;
}

FLAC__StreamDecoderWriteStatus flacWrite(const FLAC__FileDecoder *dec, const FLAC__Frame *frame, const FLAC__int32 * const buf[], void * vdata) {
	FlacData * data = (FlacData *)vdata;
	FLAC__uint32 samples = frame->header.blocksize;
	FLAC__uint16 u16;
	unsigned char * uc;
	int c_samp, c_chan, d_samp;
	int i;
	float timeChange;
	FLAC__uint64 newPosition = 0;
	
	timeChange = ((float)samples)/frame->header.sample_rate;
	data->time+= timeChange;

	FLAC__file_decoder_get_decode_position(dec,&newPosition);
	if(data->position) {
		data->bitRate = ((newPosition-data->position)*8.0/timeChange)
				/1024+0.5;
	}
	data->position = newPosition;

	for(c_samp = d_samp = 0; c_samp < frame->header.blocksize; c_samp++) {
		for(c_chan = 0; c_chan < frame->header.channels; 
				c_chan++, d_samp++) {
			u16 = buf[c_chan][c_samp];
			uc = (unsigned char *)&u16;
			for(i=0;i<(data->af->bits/8);i++) {
				if(data->chunk_length>=CHUNK_SIZE) {
					if(flacSendChunk(data)<0) {
						return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
					}
					data->chunk_length = 0;
					if(data->dc->seek) {
						return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
					}
				}
				data->chunk[data->chunk_length++] = *(uc++);
			}
		}
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

int flac_getAudioFormatAndTime(char * file, AudioFormat * format, float * time) {
	FLAC__Metadata_SimpleIterator * it;
	FLAC__StreamMetadata * block = NULL;
	int found = 0;
	int ret = -1;

	if(!(it = FLAC__metadata_simple_iterator_new())) return -1;
        if(!FLAC__metadata_simple_iterator_init(it,file,1,0)) {
                FLAC__metadata_simple_iterator_delete(it);
	        return -1;
	}

        do {
                if(block) FLAC__metadata_object_delete(block);
                block = FLAC__metadata_simple_iterator_get_block(it);
                if(block->type == FLAC__METADATA_TYPE_STREAMINFO) found=1;
        } while(!found && FLAC__metadata_simple_iterator_next(it));

	if(found) {
		format->bits = block->data.stream_info.bits_per_sample;
		format->bits = 16;
		format->sampleRate = block->data.stream_info.sample_rate;
		format->channels = block->data.stream_info.channels;
		*time = ((float)block->data.stream_info.total_samples)/
			format->sampleRate;
		ret = 0;
	}

	if(block) FLAC__metadata_object_delete(block);
	FLAC__metadata_simple_iterator_delete(it);

	return ret;
}

int flac_decode(Buffer * cb, AudioFormat * af, DecoderControl * dc) {
	if(flac_getAudioFormatAndTime(dc->file,af,&(cb->totalTime))<0) {
		ERROR("\"%s\" doesn't seem to be a flac\n",dc->file);
		return -1;
	}

	dc->state = DECODE_STATE_DECODE;
	dc->start = 0;

	flacPlayFile(dc->file,cb,af,dc);

	if(dc->seek) dc->seek = 0;
	
	if(dc->stop) {
		dc->state = DECODE_STATE_STOP;
		dc->stop = 0;
	}
	else dc->state = DECODE_STATE_STOP;

	return 0;
}

#endif
