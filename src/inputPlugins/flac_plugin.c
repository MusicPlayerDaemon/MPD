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

#include "../inputPlugin.h"

#ifdef HAVE_FLAC

#include "../utils.h"
#include "../log.h"
#include "../pcm_utils.h"
#include "../inputStream.h"
#include "../outputBuffer.h"
#include "../replayGain.h"
#include "../audio.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <FLAC/seekable_stream_decoder.h>
#include <FLAC/metadata.h>

typedef struct {
#define FLAC_CHUNK_SIZE 4080
	unsigned char chunk[FLAC_CHUNK_SIZE];
	int chunk_length;
	float time;
	int bitRate;
	FLAC__uint64 position;
	OutputBuffer * cb;
	DecoderControl * dc;
        InputStream * inStream;
	ReplayGainInfo * replayGainInfo;
} FlacData;

/* this code is based on flac123, from flac-tools */

int flacSendChunk(FlacData * data);
void flacError(const FLAC__SeekableStreamDecoder *, 
                FLAC__StreamDecoderErrorStatus, void *);
void flacPrintErroredState(FLAC__SeekableStreamDecoderState state);
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

int flac_decode(OutputBuffer * cb, DecoderControl * dc, InputStream * inStream)
{
	FLAC__SeekableStreamDecoder * flacDec = NULL;
	FlacData data;
	int status = 1;
        int ret =0;

	data.chunk_length = 0;
	data.time = 0;
	data.position = 0;
	data.bitRate = 0;
	data.cb = cb;
	data.dc = dc;
	data.inStream = inStream;
	data.replayGainInfo = NULL;

	if(!(flacDec = FLAC__seekable_stream_decoder_new())) {
                ret = -1;
                goto fail;
        }
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
        status&=FLAC__seekable_stream_decoder_set_metadata_respond(flacDec,
			FLAC__METADATA_TYPE_VORBIS_COMMENT);
	status&=FLAC__seekable_stream_decoder_set_error_callback(flacDec,
                        flacError);
	status&=FLAC__seekable_stream_decoder_set_client_data(flacDec,
                        (void *)&data);
	if(!status) {
		ERROR("flac problem before init()\n");
		flacPrintErroredState(
                        FLAC__seekable_stream_decoder_get_state(flacDec));
		ret = -1;
                goto fail;
	}

	if(FLAC__seekable_stream_decoder_init(flacDec)!=
			FLAC__STREAM_DECODER_SEARCH_FOR_METADATA) 
	{
		ERROR("flac problem doing init()\n");
		flacPrintErroredState(
                        FLAC__seekable_stream_decoder_get_state(flacDec));
                ret = -1;
                goto fail;
	}

	if(!FLAC__seekable_stream_decoder_process_until_end_of_metadata(flacDec)) {
		ERROR("flac problem reading metadata\n");
		flacPrintErroredState(
                        FLAC__seekable_stream_decoder_get_state(flacDec));
		ret = -1;
                goto fail;
	}

	dc->state = DECODE_STATE_DECODE;

	while(1) {
		FLAC__seekable_stream_decoder_process_single(flacDec);
		if(FLAC__seekable_stream_decoder_get_state(flacDec)!=
				FLAC__SEEKABLE_STREAM_DECODER_OK)
		{
			break;
		}
		if(dc->seek) {
			FLAC__uint64 sampleToSeek = dc->seekWhere*
					dc->audioFormat.sampleRate+0.5;
			if(FLAC__seekable_stream_decoder_seek_absolute(flacDec,
						sampleToSeek))
			{
                                clearOutputBuffer(cb);
				data.time = ((float)sampleToSeek)/
					dc->audioFormat.sampleRate;
				data.position = 0;
			}
                        else dc->seekError = 1;
			dc->seek = 0;
		}
	}
	/* I don't think we need this bit here! -shank */
	/*FLAC__file_decoder_process_until_end_of_file(flacDec);*/
	if(!dc->stop) {
		flacPrintErroredState(
                        FLAC__seekable_stream_decoder_get_state(flacDec));
		FLAC__seekable_stream_decoder_finish(flacDec);
	}
	/* send last little bit */
	if(data.chunk_length>0 && !dc->stop) {
		flacSendChunk(&data);
		flushOutputBuffer(data.cb);
	}

	/*if(dc->seek) {
                dc->seekError = 1;
                dc->seek = 0;
        } */
	
	if(dc->stop) {
		dc->state = DECODE_STATE_STOP;
		dc->stop = 0;
	}
	else dc->state = DECODE_STATE_STOP;

fail:
	if(data.replayGainInfo) freeReplayGainInfo(data.replayGainInfo);

	closeInputStream(inStream);
	
	if(flacDec) FLAC__seekable_stream_decoder_delete(flacDec);

	return ret;
}

FLAC__SeekableStreamDecoderReadStatus flacRead(
                const FLAC__SeekableStreamDecoder * flacDec, FLAC__byte buf[],
                unsigned * bytes, void * fdata) {
	FlacData * data = (FlacData *) fdata;

        *bytes = readFromInputStream(data->inStream,(void *)buf,1,*bytes);

        if(*bytes==0) return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR;
        
        return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK;
}

FLAC__SeekableStreamDecoderSeekStatus flacSeek(
                const FLAC__SeekableStreamDecoder * flacDec, 
                FLAC__uint64 offset, void * fdata) 
{
	FlacData * data = (FlacData *) fdata;

        if(seekInputStream(data->inStream,offset,SEEK_SET)<0) {
                return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;
        }

        return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK;
}

FLAC__SeekableStreamDecoderTellStatus flacTell(
                const FLAC__SeekableStreamDecoder * flacDec, 
                FLAC__uint64 * offset, void * fdata) 
{
	FlacData * data = (FlacData *) fdata;

        *offset = (long)(data->inStream->offset);

        return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__SeekableStreamDecoderLengthStatus flacLength(
                const FLAC__SeekableStreamDecoder * flacDec, 
                FLAC__uint64 * length, void * fdata)
{
	FlacData * data = (FlacData *) fdata;

        *length = (size_t)(data->inStream->size);

        return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK;
}

FLAC__bool flacEOF(const FLAC__SeekableStreamDecoder * flacDec, void * fdata) {
	FlacData * data = (FlacData *) fdata;

        switch(inputStreamAtEOF(data->inStream)) {
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
		ERROR("flac lost sync\n");
		break;
	case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
		ERROR("bad header\n");
		break;
	case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
		ERROR("crc mismatch\n");
		break;
	default:
		ERROR("unknown flac error\n");
	}
}

void flacPrintErroredState(FLAC__SeekableStreamDecoderState state) 
{
	switch(state) {
	case FLAC__SEEKABLE_STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
		ERROR("flac allocation error\n");
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_READ_ERROR:
		ERROR("flac read error\n");
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_SEEK_ERROR:
		ERROR("flac seek error\n");
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_STREAM_DECODER_ERROR:
		ERROR("flac seekable stream error\n");
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_ALREADY_INITIALIZED:
		ERROR("flac decoder already initialized\n");
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_INVALID_CALLBACK:
		ERROR("invalid flac callback\n");
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_UNINITIALIZED:
		ERROR("flac decoder uninitialized\n");
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_OK:
	case FLAC__SEEKABLE_STREAM_DECODER_SEEKING:
	case FLAC__SEEKABLE_STREAM_DECODER_END_OF_STREAM:
		break;
	}
}

int flacFindVorbisCommentFloat(const FLAC__StreamMetadata * block, char * cmnt, 
                float * fl)
{
        int offset = FLAC__metadata_object_vorbiscomment_find_entry_from(
                        block,0,cmnt);

        if(offset >= 0) {
                int pos = strlen(cmnt)+1; /* 1 is for '=' */
                int len = block->data.vorbis_comment.comments[offset].length
                                -pos;
                if(len > 0) {
                        char * dup = malloc(len+1);
                        memcpy(dup,&(block->data.vorbis_comment.comments[offset].entry[pos]),len);
                        dup[len] = '\0';
                        *fl = atof(dup);
                        free(dup);
                        return 1;
                }
        }

        return 0;
}

/* replaygain stuff by AliasMrJones */
void flacParseReplayGain(const FLAC__StreamMetadata *block, FlacData * data) {
	int found = 0;

	if(NULL != data->replayGainInfo) {
		freeReplayGainInfo(data->replayGainInfo);
		data->replayGainInfo = NULL;
	}

	data->replayGainInfo = newReplayGainInfo();

        found &= flacFindVorbisCommentFloat(block,"replaygain_album_gain",
					&data->replayGainInfo->albumGain);
        found &= flacFindVorbisCommentFloat(block,"replaygain_album_peak",
                                	&data->replayGainInfo->albumPeak);
        found &= flacFindVorbisCommentFloat(block,"replaygain_track_gain",
					&data->replayGainInfo->trackGain);
        found &= flacFindVorbisCommentFloat(block,"replaygain_track_peak",
                                	&data->replayGainInfo->trackPeak);

	if(!found) {
		freeReplayGainInfo(data->replayGainInfo);
		data->replayGainInfo = NULL;
	}
}

void flacMetadata(const FLAC__SeekableStreamDecoder *dec, 
                const FLAC__StreamMetadata *block, void *vdata) 
{
	FlacData * data = (FlacData *)vdata;

        switch(block->type) {
        case FLAC__METADATA_TYPE_STREAMINFO:
		data->dc->audioFormat.bits = 
                                block->data.stream_info.bits_per_sample;
		data->dc->audioFormat.sampleRate = 
                                block->data.stream_info.sample_rate;
		data->dc->audioFormat.channels = 
                                block->data.stream_info.channels;
		data->dc->totalTime = 
                                ((float)block->data.stream_info.total_samples)/
			        data->dc->audioFormat.sampleRate;
                getOutputAudioFormat(&(data->dc->audioFormat),
                                &(data->cb->audioFormat));
                break;
        case FLAC__METADATA_TYPE_VORBIS_COMMENT:
                flacParseReplayGain(block,data);
        default:
                break;
        }
}

int flacSendChunk(FlacData * data) {
	switch(sendDataToOutputBuffer(data->cb, NULL, data->dc, 1, data->chunk,
			data->chunk_length, data->time, data->bitRate,
			data->replayGainInfo)) 
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
	int c_samp;
	int c_chan;
	float timeChange;
	FLAC__uint64 newPosition = 0;
	const int bytesPerSample = data->dc->audioFormat.bits/8;
	
	timeChange = ((float)samples)/frame->header.sample_rate;
	data->time+= timeChange;

	FLAC__seekable_stream_decoder_get_decode_position(dec,&newPosition);
	if(data->position) {
		data->bitRate = ((newPosition-data->position)*8.0/timeChange)
				/1000+0.5;
	}
	data->position = newPosition;

	for(c_samp = 0; c_samp < frame->header.blocksize; c_samp++) {
		for(c_chan = 0; c_chan < frame->header.channels; c_chan++) {
#ifdef WORDS_BIGENDIAN
			memcpy(data->chunk+data->chunk_length,
			       &buf[c_chan][c_samp]+4-bytesPerSample, 
			       bytesPerSample);
			data->chunk_length+=bytesPerSample;
#else
			memcpy(data->chunk+data->chunk_length,
			       &buf[c_chan][c_samp], bytesPerSample);
			data->chunk_length+=bytesPerSample;
#endif
			if(FLAC_CHUNK_SIZE-data->chunk_length < bytesPerSample) 
			{
				if(flacSendChunk(data)<0) {
					return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
				}
				data->chunk_length = 0;
				if(data->dc->seek) {
					return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
				}
			}
		}
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static int commentMatchesAddToTag(
		char * str,
		FLAC__StreamMetadata_VorbisComment_Entry * entry,
		int itemType,
		MpdTag ** tag)
{
	int slen = strlen(str);
	int vlen = entry->length - slen;

	if( vlen <= 0 ) return 0;

	if( 0 == strncasecmp(str, entry->entry, slen) ) {
		if(*tag == NULL) *tag = newMpdTag();
		addItemToMpdTagWithLen(*tag, itemType, 
				entry->entry+slen, vlen);
		return 1;
	}

	return 0;
}
		

static MpdTag * copyVorbisCommentBlockToMpdTag(FLAC__StreamMetadata * block, 
		MpdTag * tag)
{
	int i;

	for(i = 0; i < block->data.vorbis_comment.num_comments; i++) {
		if(commentMatchesAddToTag(
				"artist=", 
				block->data.vorbis_comment.comments+i,
				TAG_ITEM_ARTIST,
				&tag));
		else if(commentMatchesAddToTag(
				"title=", 
				block->data.vorbis_comment.comments+i,
				TAG_ITEM_TITLE,
				&tag));
		else if(commentMatchesAddToTag(
				"album=", 
				block->data.vorbis_comment.comments+i,
				TAG_ITEM_ALBUM,
				&tag));
		else if(commentMatchesAddToTag(
				"tracknumber=", 
				block->data.vorbis_comment.comments+i,
				TAG_ITEM_TRACK,
				&tag));
		else if(commentMatchesAddToTag(
				"genre=", 
				block->data.vorbis_comment.comments+i,
				TAG_ITEM_GENRE,
				&tag));
		else if(commentMatchesAddToTag(
				"date=", 
				block->data.vorbis_comment.comments+i,
				TAG_ITEM_DATE,
				&tag));
		else if(commentMatchesAddToTag(
				"composer=", 
				block->data.vorbis_comment.comments+i,
				TAG_ITEM_COMPOSER,
				&tag));
		else if(commentMatchesAddToTag(
				"performer=", 
				block->data.vorbis_comment.comments+i,
				TAG_ITEM_PERFORMER,
				&tag));
	}

	return tag;
}

MpdTag * flacMetadataDup(char * file, int * vorbisCommentFound) {
	MpdTag * ret = NULL;
	FLAC__Metadata_SimpleIterator * it;
	FLAC__StreamMetadata * block = NULL;

	*vorbisCommentFound = 0;

	it = FLAC__metadata_simple_iterator_new();
	if(!FLAC__metadata_simple_iterator_init(it, file ,1,0)) {
		FLAC__metadata_simple_iterator_delete(it);
		return ret;
	}
	
	do {
		block = FLAC__metadata_simple_iterator_get_block(it);
		if(!block) break;
		if(block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
			ret = copyVorbisCommentBlockToMpdTag(block, ret);

			if(ret) *vorbisCommentFound = 1;
		}
		else if(block->type == FLAC__METADATA_TYPE_STREAMINFO) {
			if(!ret) ret = newMpdTag();
			ret->time = ((float)block->data.stream_info.
					total_samples) /
					block->data.stream_info.sample_rate +
					0.5;
		}
		FLAC__metadata_object_delete(block);
	} while(FLAC__metadata_simple_iterator_next(it));

	FLAC__metadata_simple_iterator_delete(it);
	return ret;
}

MpdTag * flacTagDup(char * file) {
	MpdTag * ret = NULL;
	int foundVorbisComment = 0;

	ret = flacMetadataDup(file, &foundVorbisComment);
	if(!ret) return NULL;
	if(!foundVorbisComment) {
		MpdTag * temp = id3Dup(file);
		if(temp) {
			temp->time = ret->time;
			freeMpdTag(ret);
			ret = temp;
		}
	}

	return ret;
}

char * flacSuffixes[] = {"flac", NULL};
char * flac_mime_types[] = {"application/x-flac", NULL};

InputPlugin flacPlugin = 
{
        "flac",
        NULL,
	NULL,
        flac_decode,
	NULL,
        flacTagDup,
        INPUT_PLUGIN_STREAM_URL | INPUT_PLUGIN_STREAM_FILE,
        flacSuffixes,
        flac_mime_types
};

#else

InputPlugin flacPlugin =
{       
        NULL,
        NULL,
        NULL,
        NULL,
	NULL,
	NULL,
        0,
        NULL,
        NULL,
};

#endif
