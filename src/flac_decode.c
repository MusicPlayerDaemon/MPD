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

#include "flac_decode.h"

#ifdef HAVE_FLAC

#include "utils.h"
#include "log.h"
#include "pcm_utils.h"
#include "inputStream.h"
#include "outputBuffer.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <FLAC/seekable_stream_decoder.h>
#include <FLAC/metadata.h>

typedef struct {
	unsigned char chunk[CHUNK_SIZE];
	int chunk_length;
	float time;
	int bitRate;
	FLAC__uint64 position;
	OutputBuffer * cb;
	AudioFormat * af;
	DecoderControl * dc;
        char * file;
        InputStream inStream;
} FlacData;

/* this code is based on flac123, from flac-tools */

int flacSendChunk(FlacData * data);
void flacError(const FLAC__SeekableStreamDecoder *, 
                FLAC__StreamDecoderErrorStatus, void *);
void flacPrintErroredState(FLAC__SeekableStreamDecoderState state, char * file);
void flacMetadata(const FLAC__SeekableStreamDecoder *, 
                const FLAC__StreamMetadata *, void *);
FLAC__StreamDecoderWriteStatus flacWrite(const FLAC__SeekableStreamDecoder *,
                const FLAC__Frame *, const FLAC__int32 * const buf[], void *);
FLAC__SeekableStreamDecoderReadStatus flacRead(
                const FLAC__SeekableStreamDecoder *, FLAC__byte buf[],
                unsigned *, void *);
FLAC__SeekableStreamDecoderSeekStatus flacSeek(
                const FLAC__SeekableStreamDecoder *, FLAC__uint64, void *);
FLAC__SeekableStreamDecoderTellStatus flacTell(
                const FLAC__SeekableStreamDecoder *, FLAC__uint64 *, void *);
FLAC__SeekableStreamDecoderLengthStatus flacLength(
                const FLAC__SeekableStreamDecoder *, FLAC__uint64 *, void *);
FLAC__bool flacEOF(const FLAC__SeekableStreamDecoder *, void *);

void flacPlayFile(char *file, OutputBuffer * cb, AudioFormat * af, 
	DecoderControl *dc)
{
	FLAC__SeekableStreamDecoder * flacDec;
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

        if(openInputStreamFromFile(&(data.inStream),file)<0) {
                ERROR("unable to open flac: %s\n",file);
                return;
        }
	
	if(!(flacDec = FLAC__seekable_stream_decoder_new())) return;
	/*status&=FLAC__file_decoder_set_md5_checking(flacDec,1);*/
	status&=FLAC__seekable_stream_decoder_set_read_callback(flacDec,
                        flacRead);
	status&=FLAC__seekable_stream_decoder_set_seek_callback(flacDec,
                        flacSeek);
	status&=FLAC__seekable_stream_decoder_set_tell_callback(flacDec,
                        flacTell);
	status&=FLAC__seekable_stream_decoder_set_length_callback(flacDec,
                        flacLength);
	status&=FLAC__seekable_stream_decoder_set_eof_callback(flacDec,flacEOF);
	status&=FLAC__seekable_stream_decoder_set_write_callback(flacDec,
                        flacWrite);
	status&=FLAC__seekable_stream_decoder_set_metadata_callback(flacDec,
                        flacMetadata);
	status&=FLAC__seekable_stream_decoder_set_error_callback(flacDec,
                        flacError);
	status&=FLAC__seekable_stream_decoder_set_client_data(flacDec,
                        (void *)&data);
	if(!status) {
		ERROR("flac problem before init(): %s\n",file);
		flacPrintErroredState(
                        FLAC__seekable_stream_decoder_get_state(flacDec),file);
		FLAC__seekable_stream_decoder_delete(flacDec);
		return;
	}
	if(FLAC__seekable_stream_decoder_init(flacDec)!=
			FLAC__STREAM_DECODER_SEARCH_FOR_METADATA) 
	{
		ERROR("flac problem doing init(): %s\n",file);
		flacPrintErroredState(
                        FLAC__seekable_stream_decoder_get_state(flacDec),file);
		FLAC__seekable_stream_decoder_delete(flacDec);
		return;
	}
	if(!FLAC__seekable_stream_decoder_process_until_end_of_metadata(flacDec)) {
		ERROR("flac problem reading metadata: %s\n",file);
		flacPrintErroredState(
                        FLAC__seekable_stream_decoder_get_state(flacDec),file);
		FLAC__seekable_stream_decoder_delete(flacDec);
		return;
	}
	while(1) {
		FLAC__seekable_stream_decoder_process_single(flacDec);
		if(FLAC__seekable_stream_decoder_get_state(flacDec)!=
				FLAC__SEEKABLE_STREAM_DECODER_OK)
		{
			break;
		}
		if(dc->seek) {
			FLAC__uint64 sampleToSeek = dc->seekWhere*
					af->sampleRate+0.5;
			cb->end = cb->begin;
			cb->wrap = 0;
			if(FLAC__seekable_stream_decoder_seek_absolute(flacDec,
						sampleToSeek))
			{
				data.time = ((float)sampleToSeek)/
					af->sampleRate;
				data.position = 0;
			}
			dc->seek = 0;
		}
	}
	/* I don't think we need this bit here! -shank */
	/*FLAC__file_decoder_process_until_end_of_file(flacDec);*/
	if(!dc->stop) {
		flacPrintErroredState(
                        FLAC__seekable_stream_decoder_get_state(flacDec),file);
		FLAC__seekable_stream_decoder_finish(flacDec);
	}
	FLAC__seekable_stream_decoder_delete(flacDec);
	/* send last little bit */
	if(data.chunk_length>0 && !dc->stop) {
		flacSendChunk(&data);
		flushOutputBuffer(data.cb);
	}
}

FLAC__SeekableStreamDecoderReadStatus flacRead(
                const FLAC__SeekableStreamDecoder * flacDec, FLAC__byte buf[],
                unsigned * bytes, void * fdata) {
	FlacData * data = (FlacData *) fdata;

        *bytes = readFromInputStream(&(data->inStream),(void *)buf,1,*bytes);

        if(*bytes==0) return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR;
        
        return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK;
}

FLAC__SeekableStreamDecoderSeekStatus flacSeek(
                const FLAC__SeekableStreamDecoder * flacDec, 
                FLAC__uint64 offset, void * fdata) 
{
	FlacData * data = (FlacData *) fdata;

        if(seekInputStream(&(data->inStream),offset,SEEK_SET)<0) {
                return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;
        }

        return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK;
}

FLAC__SeekableStreamDecoderTellStatus flacTell(
                const FLAC__SeekableStreamDecoder * flacDec, 
                FLAC__uint64 * offset, void * fdata) 
{
	FlacData * data = (FlacData *) fdata;

        *offset = data->inStream.offset;

        return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__SeekableStreamDecoderLengthStatus flacLength(
                const FLAC__SeekableStreamDecoder * flacDec, 
                FLAC__uint64 * length, void * fdata)
{
	FlacData * data = (FlacData *) fdata;

        *length = data->inStream.size;

        return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK;
}

FLAC__bool flacEOF(const FLAC__SeekableStreamDecoder * flacDec, void * fdata) {
	FlacData * data = (FlacData *) fdata;

        switch(inputStreamAtEOF(&(data->inStream))) {
        case 1:
                return true;
        default:
                return false;
        }
}

void flacError(const FLAC__SeekableStreamDecoder *dec, 
                FLAC__StreamDecoderErrorStatus status, void *fdata) 
{
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

void flacPrintErroredState(FLAC__SeekableStreamDecoderState state, 
                char * file) 
{
	switch(state) {
	case FLAC__SEEKABLE_STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
		ERROR("flac allocation error\n");
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_READ_ERROR:
		ERROR("flac read error: %s\n",file);
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_SEEK_ERROR:
		ERROR("flac seek error: %s\n",file);
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_STREAM_DECODER_ERROR:
		ERROR("flac seekable stream error: %s\n",file);
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_ALREADY_INITIALIZED:
		ERROR("flac decoder already initilaized: %s\n",file);
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_INVALID_CALLBACK:
		ERROR("invalid flac callback\n");
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_UNINITIALIZED:
		ERROR("flac decoder uninitialized: %s\n",file);
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_OK:
	case FLAC__SEEKABLE_STREAM_DECODER_SEEKING:
	case FLAC__SEEKABLE_STREAM_DECODER_END_OF_STREAM:
		break;
	}
}

void flacMetadata(const FLAC__SeekableStreamDecoder *dec, 
                const FLAC__StreamMetadata *meta, void *data) 
{
}

int flacSendChunk(FlacData * data) {
	switch(sendDataToOutputBuffer(data->cb,data->dc,data->chunk,
			data->chunk_length,data->time,data->bitRate)) 
	{
	case OUTPUT_BUFFER_DC_STOP:
		return -1;
	default:
		return 0;
	}

	return 0;
}

FLAC__StreamDecoderWriteStatus flacWrite(const FLAC__SeekableStreamDecoder *dec,
                const FLAC__Frame *frame, const FLAC__int32 * const buf[], 
                void * vdata) 
{
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

	FLAC__seekable_stream_decoder_get_decode_position(dec,&newPosition);
	if(data->position) {
		data->bitRate = ((newPosition-data->position)*8.0/timeChange)
				/1000+0.5;
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

int getFlacTotalTime(char * file) {
	float totalTime;
	AudioFormat af;

	if(flac_getAudioFormatAndTime(file,&af,&totalTime)<0) return -1;

	return (int)(totalTime+0.5);
}

int flac_decode(OutputBuffer * cb, AudioFormat * af, DecoderControl * dc) {
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
/* vim:set shiftwidth=4 tabstop=8 expandtab: */
