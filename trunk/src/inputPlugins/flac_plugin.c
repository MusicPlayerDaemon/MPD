/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
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

#include "_flac_common.h"

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
#include <assert.h>

/* this code was based on flac123, from flac-tools */

static flac_read_status flacRead(const flac_decoder * flacDec,
                                  FLAC__byte buf[],
				  flac_read_status_size_t *bytes,
				  void *fdata)
{
	FlacData *data = (FlacData *) fdata;
	size_t r;

	while (1) {
		r = readFromInputStream(data->inStream, (void *)buf, 1, *bytes);
		if (r == 0 && !inputStreamAtEOF(data->inStream) &&
		    !data->dc->stop)
			my_usleep(10000);
		else
			break;
	}
	*bytes = r;

	if (r == 0 && !data->dc->stop) {
		if (inputStreamAtEOF(data->inStream))
			return flac_read_status_eof;
		else
			return flac_read_status_abort;
	}
	return flac_read_status_continue;
}

static flac_seek_status flacSeek(const flac_decoder * flacDec,
				 FLAC__uint64 offset,
				 void *fdata)
{
	FlacData *data = (FlacData *) fdata;

	if (seekInputStream(data->inStream, offset, SEEK_SET) < 0) {
		return flac_seek_status_error;
	}

	return flac_seek_status_ok;
}

static flac_tell_status flacTell(const flac_decoder * flacDec,
				 FLAC__uint64 * offset,
				 void *fdata)
{
	FlacData *data = (FlacData *) fdata;

	*offset = (long)(data->inStream->offset);

	return flac_tell_status_ok;
}

static flac_length_status flacLength(const flac_decoder * flacDec,
				     FLAC__uint64 * length,
				     void *fdata)
{
	FlacData *data = (FlacData *) fdata;

	*length = (size_t) (data->inStream->size);

	return flac_length_status_ok;
}

static FLAC__bool flacEOF(const flac_decoder * flacDec, void *fdata)
{
	FlacData *data = (FlacData *) fdata;

	if (inputStreamAtEOF(data->inStream) == 1)
		return true;
	return false;
}

static void flacError(const flac_decoder *dec,
		      FLAC__StreamDecoderErrorStatus status, void *fdata)
{
	flac_error_common_cb("flac", status, (FlacData *) fdata);
}

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
static void flacPrintErroredState(FLAC__SeekableStreamDecoderState state)
{
	const char *str = ""; /* "" to silence compiler warning */
	switch (state) {
	case FLAC__SEEKABLE_STREAM_DECODER_OK:
	case FLAC__SEEKABLE_STREAM_DECODER_SEEKING:
	case FLAC__SEEKABLE_STREAM_DECODER_END_OF_STREAM:
		return;
	case FLAC__SEEKABLE_STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
		str = "allocation error";
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_READ_ERROR:
		str = "read error";
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_SEEK_ERROR:
		str = "seek error";
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_STREAM_DECODER_ERROR:
		str = "seekable stream error";
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_ALREADY_INITIALIZED:
		str = "decoder already initialized";
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_INVALID_CALLBACK:
		str = "invalid callback";
		break;
	case FLAC__SEEKABLE_STREAM_DECODER_UNINITIALIZED:
		str = "decoder uninitialized";
	}
	ERROR("flac %s\n", str);
}

static int flac_init(FLAC__SeekableStreamDecoder *dec,
                     FLAC__SeekableStreamDecoderReadCallback read_cb,
                     FLAC__SeekableStreamDecoderSeekCallback seek_cb,
                     FLAC__SeekableStreamDecoderTellCallback tell_cb,
                     FLAC__SeekableStreamDecoderLengthCallback length_cb,
                     FLAC__SeekableStreamDecoderEofCallback eof_cb,
                     FLAC__SeekableStreamDecoderWriteCallback write_cb,
                     FLAC__SeekableStreamDecoderMetadataCallback metadata_cb,
                     FLAC__SeekableStreamDecoderErrorCallback error_cb,
                     void *data)
{
	int s = 1;
	s &= FLAC__seekable_stream_decoder_set_read_callback(dec, read_cb);
	s &= FLAC__seekable_stream_decoder_set_seek_callback(dec, seek_cb);
	s &= FLAC__seekable_stream_decoder_set_tell_callback(dec, tell_cb);
	s &= FLAC__seekable_stream_decoder_set_length_callback(dec, length_cb);
	s &= FLAC__seekable_stream_decoder_set_eof_callback(dec, eof_cb);
	s &= FLAC__seekable_stream_decoder_set_write_callback(dec, write_cb);
	s &= FLAC__seekable_stream_decoder_set_metadata_callback(dec,
	                                                         metadata_cb);
	s &= FLAC__seekable_stream_decoder_set_metadata_respond(dec,
	                                  FLAC__METADATA_TYPE_VORBIS_COMMENT);
	s &= FLAC__seekable_stream_decoder_set_error_callback(dec, error_cb);
	s &= FLAC__seekable_stream_decoder_set_client_data(dec, data);
	if (!s || (FLAC__seekable_stream_decoder_init(dec) !=
	           FLAC__SEEKABLE_STREAM_DECODER_OK))
		return 0;
	return 1;
}
#else /* FLAC_API_VERSION_CURRENT >= 7 */
static void flacPrintErroredState(FLAC__StreamDecoderState state)
{
	const char *str = ""; /* "" to silence compiler warning */
	switch (state) {
	case FLAC__STREAM_DECODER_SEARCH_FOR_METADATA:
	case FLAC__STREAM_DECODER_READ_METADATA:
	case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC:
	case FLAC__STREAM_DECODER_READ_FRAME:
	case FLAC__STREAM_DECODER_END_OF_STREAM:
		return;
	case FLAC__STREAM_DECODER_OGG_ERROR:
		str = "error in the Ogg layer";
		break;
	case FLAC__STREAM_DECODER_SEEK_ERROR:
		str = "seek error";
		break;
	case FLAC__STREAM_DECODER_ABORTED:
		str = "decoder aborted by read";
		break;
	case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
		str = "allocation error";
		break;
	case FLAC__STREAM_DECODER_UNINITIALIZED:
		str = "decoder uninitialized";
	}
	ERROR("flac %s\n", str);
}
#endif /* FLAC_API_VERSION_CURRENT >= 7 */

static void flacMetadata(const flac_decoder * dec,
			 const FLAC__StreamMetadata * block, void *vdata)
{
	flac_metadata_common_cb(block, (FlacData *) vdata);
}

static FLAC__StreamDecoderWriteStatus flacWrite(const flac_decoder *dec,
                                                const FLAC__Frame * frame,
						const FLAC__int32 * const buf[],
						void *vdata)
{
	FlacData *data = (FlacData *) vdata;
	FLAC__uint32 samples = frame->header.blocksize;
	FLAC__uint16 u16;
	unsigned char *uc;
	int c_samp, c_chan, d_samp;
	int i;
	float timeChange;
	FLAC__uint64 newPosition = 0;

	timeChange = ((float)samples) / frame->header.sample_rate;
	data->time += timeChange;

	flac_get_decode_position(dec, &newPosition);
	if (data->position) {
		data->bitRate =
		    ((newPosition - data->position) * 8.0 / timeChange)
		    / 1000 + 0.5;
	}
	data->position = newPosition;

	for (c_samp = d_samp = 0; c_samp < frame->header.blocksize; c_samp++) {
		for (c_chan = 0; c_chan < frame->header.channels;
		     c_chan++, d_samp++) {
			u16 = buf[c_chan][c_samp];
			uc = (unsigned char *)&u16;
			for (i = 0; i < (data->dc->audioFormat.bits / 8); i++) {
				if (data->chunk_length >= FLAC_CHUNK_SIZE) {
					if (flacSendChunk(data) < 0) {
						return
						    FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
					}
					data->chunk_length = 0;
					if (data->dc->seek) {
						return
						    FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
					}
				}
				data->chunk[data->chunk_length++] = *(uc++);
			}
		}
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static MpdTag *flacMetadataDup(char *file, int *vorbisCommentFound)
{
	MpdTag *ret = NULL;
	FLAC__Metadata_SimpleIterator *it;
	FLAC__StreamMetadata *block = NULL;

	*vorbisCommentFound = 0;

	it = FLAC__metadata_simple_iterator_new();
	if (!FLAC__metadata_simple_iterator_init(it, file, 1, 0)) {
		switch (FLAC__metadata_simple_iterator_status(it)) {
		case FLAC__METADATA_SIMPLE_ITERATOR_STATUS_ILLEGAL_INPUT:
			DEBUG
			    ("flacMetadataDup: Reading '%s' metadata gave the following error: Illegal Input\n",
			     file);
			break;
		case FLAC__METADATA_SIMPLE_ITERATOR_STATUS_ERROR_OPENING_FILE:
			DEBUG
			    ("flacMetadataDup: Reading '%s' metadata gave the following error: Error Opening File\n",
			     file);
			break;
		case FLAC__METADATA_SIMPLE_ITERATOR_STATUS_NOT_A_FLAC_FILE:
			DEBUG
			    ("flacMetadataDup: Reading '%s' metadata gave the following error: Not A Flac File\n",
			     file);
			break;
		default:
			DEBUG("flacMetadataDup: Reading '%s' metadata failed\n",
			      file);
		}
		FLAC__metadata_simple_iterator_delete(it);
		return ret;
	}

	do {
		block = FLAC__metadata_simple_iterator_get_block(it);
		if (!block)
			break;
		if (block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
			ret = copyVorbisCommentBlockToMpdTag(block, ret);

			if (ret)
				*vorbisCommentFound = 1;
		} else if (block->type == FLAC__METADATA_TYPE_STREAMINFO) {
			if (!ret)
				ret = newMpdTag();
			ret->time = ((float)block->data.stream_info.
				     total_samples) /
			    block->data.stream_info.sample_rate + 0.5;
		}
		FLAC__metadata_object_delete(block);
	} while (FLAC__metadata_simple_iterator_next(it));

	FLAC__metadata_simple_iterator_delete(it);
	return ret;
}

static MpdTag *flacTagDup(char *file)
{
	MpdTag *ret = NULL;
	int foundVorbisComment = 0;

	ret = flacMetadataDup(file, &foundVorbisComment);
	if (!ret) {
		DEBUG("flacTagDup: Failed to grab information from: %s\n",
		      file);
		return NULL;
	}
	if (!foundVorbisComment) {
		MpdTag *temp = id3Dup(file);
		if (temp) {
			temp->time = ret->time;
			freeMpdTag(ret);
			ret = temp;
		}
	}

	return ret;
}

static int flac_decode_internal(OutputBuffer * cb, DecoderControl * dc,
                               InputStream * inStream, int is_ogg)
{
	flac_decoder *flacDec;
	FlacData data;
	const char *err = NULL;

	if (!(flacDec = flac_new()))
		return -1;
	init_FlacData(&data, cb, dc, inStream);
	if (is_ogg) {
		if (!flac_ogg_init(flacDec, flacRead, flacSeek, flacTell,
		                   flacLength, flacEOF, flacWrite, flacMetadata,
			           flacError, (void *)&data)) {
			err = "doing Ogg init()";
			goto fail;
		}
	} else {
		if (!flac_init(flacDec, flacRead, flacSeek, flacTell,
		               flacLength, flacEOF, flacWrite, flacMetadata,
			       flacError, (void *)&data)) {
			err = "doing init()";
			goto fail;
		}
		if (!flac_process_metadata(flacDec)) {
			err = "problem reading metadata";
			goto fail;
		}
	}

	dc->state = DECODE_STATE_DECODE;

	while (1) {
		if (!flac_process_single(flacDec))
			break;
		if (flac_get_state(flacDec) == flac_decoder_eof)
			break;
		if (dc->seek) {
			FLAC__uint64 sampleToSeek = dc->seekWhere *
			    dc->audioFormat.sampleRate + 0.5;
			if (flac_seek_absolute(flacDec, sampleToSeek)) {
				clearOutputBuffer(cb);
				data.time = ((float)sampleToSeek) /
				    dc->audioFormat.sampleRate;
				data.position = 0;
			} else
				dc->seekError = 1;
			dc->seek = 0;
		}
	}
	if (!dc->stop) {
		flacPrintErroredState(flac_get_state(flacDec));
		flac_finish(flacDec);
	}
	/* send last little bit */
	if (data.chunk_length > 0 && !dc->stop) {
		flacSendChunk(&data);
		flushOutputBuffer(data.cb);
	}

	/*if(dc->seek) {
	   dc->seekError = 1;
	   dc->seek = 0;
	   } */

	dc->state = DECODE_STATE_STOP;
	dc->stop = 0;

fail:
	if (data.replayGainInfo)
		freeReplayGainInfo(data.replayGainInfo);

	if (flacDec)
		flac_delete(flacDec);

	closeInputStream(inStream);

	if (err) {
		ERROR("flac %s\n", err);
		return -1;
	}
	return 0;
}

static int flac_decode(OutputBuffer * cb, DecoderControl * dc,
                       InputStream * inStream)
{
	return flac_decode_internal(cb, dc, inStream, 0);
}

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
#  define flac_plugin_init NULL
#else /* FLAC_API_VERSION_CURRENT >= 7 */
/* some of this stuff is duplicated from oggflac_plugin.c */
extern InputPlugin oggflacPlugin;

static MpdTag *oggflac_tag_dup(char *file)
{
	MpdTag *ret = NULL;
	FLAC__Metadata_Iterator *it;
	FLAC__StreamMetadata *block;
	FLAC__Metadata_Chain *chain = FLAC__metadata_chain_new();

	if (!(FLAC__metadata_chain_read_ogg(chain, file)))
		goto out;
	it = FLAC__metadata_iterator_new();
	FLAC__metadata_iterator_init(it, chain);
	do {
		if (!(block = FLAC__metadata_iterator_get_block(it)))
			break;
		if (block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
			ret = copyVorbisCommentBlockToMpdTag(block, ret);
		} else if (block->type == FLAC__METADATA_TYPE_STREAMINFO) {
			if (!ret)
				ret = newMpdTag();
			ret->time = ((float)block->data.stream_info.
				     total_samples) /
			    block->data.stream_info.sample_rate + 0.5;
		}
	} while (FLAC__metadata_iterator_next(it));
	FLAC__metadata_iterator_delete(it);
out:
	FLAC__metadata_chain_delete(chain);
	return ret;
}

static int oggflac_decode(OutputBuffer * cb, DecoderControl * dc,
		          InputStream * inStream)
{
	return flac_decode_internal(cb, dc, inStream, 1);
}

static unsigned int oggflac_try_decode(InputStream * inStream)
{
	return (ogg_stream_type_detect(inStream) == FLAC) ? 1 : 0;
}

static char *oggflac_suffixes[] = { "ogg", NULL };
static char *oggflac_mime_types[] = { "audio/x-flac+ogg",
                                      "application/ogg",
                                      "application/x-ogg",
                                      NULL };

static int flac_plugin_init(void)
{
	if (!FLAC_API_SUPPORTS_OGG_FLAC) {
		DEBUG("libFLAC does not support OggFLAC\n");
		return 1;
	}
	DEBUG("libFLAC supports OggFLAC, initializing OggFLAC support\n");
	assert(oggflacPlugin.name == NULL);
	oggflacPlugin.name = "oggflac";
	oggflacPlugin.tryDecodeFunc = oggflac_try_decode;
	oggflacPlugin.streamDecodeFunc = oggflac_decode;
	oggflacPlugin.tagDupFunc = oggflac_tag_dup;
	oggflacPlugin.streamTypes = INPUT_PLUGIN_STREAM_URL |
	                            INPUT_PLUGIN_STREAM_FILE;
	oggflacPlugin.suffixes = oggflac_suffixes;
	oggflacPlugin.mimeTypes = oggflac_mime_types;
	loadInputPlugin(&oggflacPlugin);
	return 1;
}

#endif /* FLAC_API_VERSION_CURRENT >= 7 */

static char *flacSuffixes[] = { "flac", NULL };
static char *flac_mime_types[] = { "audio/x-flac",
                                   "application/x-flac",
                                   NULL };

InputPlugin flacPlugin = {
	"flac",
	flac_plugin_init,
	NULL,
	NULL,
	flac_decode,
	NULL,
	flacTagDup,
	INPUT_PLUGIN_STREAM_URL | INPUT_PLUGIN_STREAM_FILE,
	flacSuffixes,
	flac_mime_types
};

#else /* !HAVE_FLAC */

InputPlugin flacPlugin;

#endif /* HAVE_FLAC */
