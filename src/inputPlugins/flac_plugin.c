/* the Music Player Daemon (MPD)
 * (c)2003-2006 by Warren Dukes (warren.dukes@gmail.com)
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

#include "_flac_common.h"

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

/* this code is based on flac123, from flac-tools */

static FLAC__SeekableStreamDecoderReadStatus flacRead(const
						      FLAC__SeekableStreamDecoder
						      * flacDec,
						      FLAC__byte buf[],
						      unsigned *bytes,
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

	if (*bytes == 0 && !inputStreamAtEOF(data->inStream) && !data->dc->stop)
		return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR;

	return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK;
}

static FLAC__SeekableStreamDecoderSeekStatus flacSeek(const
						      FLAC__SeekableStreamDecoder
						      * flacDec,
						      FLAC__uint64 offset,
						      void *fdata)
{
	FlacData *data = (FlacData *) fdata;

	if (seekInputStream(data->inStream, offset, SEEK_SET) < 0) {
		return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;
	}

	return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK;
}

static FLAC__SeekableStreamDecoderTellStatus flacTell(const
						      FLAC__SeekableStreamDecoder
						      * flacDec,
						      FLAC__uint64 * offset,
						      void *fdata)
{
	FlacData *data = (FlacData *) fdata;

	*offset = (long)(data->inStream->offset);

	return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__SeekableStreamDecoderLengthStatus flacLength(const
							  FLAC__SeekableStreamDecoder
							  * flacDec,
							  FLAC__uint64 * length,
							  void *fdata)
{
	FlacData *data = (FlacData *) fdata;

	*length = (size_t) (data->inStream->size);

	return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool flacEOF(const FLAC__SeekableStreamDecoder * flacDec,
			  void *fdata)
{
	FlacData *data = (FlacData *) fdata;

	if (inputStreamAtEOF(data->inStream) == 1)
		return true;
	return false;
}

static void flacError(const FLAC__SeekableStreamDecoder * dec,
		      FLAC__StreamDecoderErrorStatus status, void *fdata)
{
	flac_error_common_cb("flac", status, (FlacData *) fdata);
}

static void flacPrintErroredState(FLAC__SeekableStreamDecoderState state)
{
	switch (state) {
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

static void flacMetadata(const FLAC__SeekableStreamDecoder * dec,
			 const FLAC__StreamMetadata * block, void *vdata)
{
	flac_metadata_common_cb(block, (FlacData *) vdata);
}

static FLAC__StreamDecoderWriteStatus flacWrite(const
						FLAC__SeekableStreamDecoder *
						dec, const FLAC__Frame * frame,
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

	FLAC__seekable_stream_decoder_get_decode_position(dec, &newPosition);
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

static int flac_decode(OutputBuffer * cb, DecoderControl * dc,
		       InputStream * inStream)
{
	FLAC__SeekableStreamDecoder *flacDec = NULL;
	FlacData data;
	int status = 1;
	int ret = 0;

	init_FlacData(&data, cb, dc, inStream);

	if (!(flacDec = FLAC__seekable_stream_decoder_new())) {
		ret = -1;
		goto fail;
	}
	/*status&=FLAC__file_decoder_set_md5_checking(flacDec,1); */
	status &= FLAC__seekable_stream_decoder_set_read_callback(flacDec,
								  flacRead);
	status &= FLAC__seekable_stream_decoder_set_seek_callback(flacDec,
								  flacSeek);
	status &= FLAC__seekable_stream_decoder_set_tell_callback(flacDec,
								  flacTell);
	status &= FLAC__seekable_stream_decoder_set_length_callback(flacDec,
								    flacLength);
	status &=
	    FLAC__seekable_stream_decoder_set_eof_callback(flacDec, flacEOF);
	status &=
	    FLAC__seekable_stream_decoder_set_write_callback(flacDec,
							     flacWrite);
	status &=
	    FLAC__seekable_stream_decoder_set_metadata_callback(flacDec,
								flacMetadata);
	status &=
	    FLAC__seekable_stream_decoder_set_metadata_respond(flacDec,
							       FLAC__METADATA_TYPE_VORBIS_COMMENT);
	status &=
	    FLAC__seekable_stream_decoder_set_error_callback(flacDec,
							     flacError);
	status &=
	    FLAC__seekable_stream_decoder_set_client_data(flacDec,
							  (void *)&data);
	if (!status) {
		ERROR("flac problem before init()\n");
		flacPrintErroredState(FLAC__seekable_stream_decoder_get_state
				      (flacDec));
		ret = -1;
		goto fail;
	}

	if (FLAC__seekable_stream_decoder_init(flacDec) !=
	    FLAC__SEEKABLE_STREAM_DECODER_OK) {
		ERROR("flac problem doing init()\n");
		flacPrintErroredState(FLAC__seekable_stream_decoder_get_state
				      (flacDec));
		ret = -1;
		goto fail;
	}

	if (!FLAC__seekable_stream_decoder_process_until_end_of_metadata
	    (flacDec)) {
		ERROR("flac problem reading metadata\n");
		flacPrintErroredState(FLAC__seekable_stream_decoder_get_state
				      (flacDec));
		ret = -1;
		goto fail;
	}

	dc->state = DECODE_STATE_DECODE;

	while (1) {
		FLAC__seekable_stream_decoder_process_single(flacDec);
		if (FLAC__seekable_stream_decoder_get_state(flacDec) !=
		    FLAC__SEEKABLE_STREAM_DECODER_OK) {
			break;
		}
		if (dc->seek) {
			FLAC__uint64 sampleToSeek = dc->seekWhere *
			    dc->audioFormat.sampleRate + 0.5;
			if (FLAC__seekable_stream_decoder_seek_absolute(flacDec,
									sampleToSeek))
			{
				clearOutputBuffer(cb);
				data.time = ((float)sampleToSeek) /
				    dc->audioFormat.sampleRate;
				data.position = 0;
			} else
				dc->seekError = 1;
			dc->seek = 0;
		}
	}
	/* I don't think we need this bit here! -shank */
	/*FLAC__file_decoder_process_until_end_of_file(flacDec); */
	if (!dc->stop) {
		flacPrintErroredState(FLAC__seekable_stream_decoder_get_state
				      (flacDec));
		FLAC__seekable_stream_decoder_finish(flacDec);
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
		FLAC__seekable_stream_decoder_delete(flacDec);

	closeInputStream(inStream);

	return ret;
}

static char *flacSuffixes[] = { "flac", NULL };
static char *flac_mime_types[] = { "application/x-flac", NULL };

InputPlugin flacPlugin = {
	"flac",
	NULL,
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

InputPlugin flacPlugin = {
	NULL,
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

#endif /* HAVE_FLAC */
