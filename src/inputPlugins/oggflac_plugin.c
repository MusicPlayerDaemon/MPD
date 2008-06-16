/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
 *
 * OggFLAC support (half-stolen from flac_plugin.c :))
 * (c) 2005 by Eric Wong <normalperson@yhbt.net>
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

#ifdef HAVE_OGGFLAC

#include "_ogg_common.h"

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

static void oggflac_cleanup(InputStream * inStream,
			    FlacData * data,
			    OggFLAC__SeekableStreamDecoder * decoder)
{
	if (data->replayGainInfo)
		freeReplayGainInfo(data->replayGainInfo);
	if (decoder)
		OggFLAC__seekable_stream_decoder_delete(decoder);
	closeInputStream(inStream);
}

static OggFLAC__SeekableStreamDecoderReadStatus of_read_cb(const
							   OggFLAC__SeekableStreamDecoder
							   * decoder,
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

	if (r == 0 && !inputStreamAtEOF(data->inStream) && !data->dc->stop)
		return OggFLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR;

	return OggFLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK;
}

static OggFLAC__SeekableStreamDecoderSeekStatus of_seek_cb(const
							   OggFLAC__SeekableStreamDecoder
							   * decoder,
							   FLAC__uint64 offset,
							   void *fdata)
{
	FlacData *data = (FlacData *) fdata;

	if (seekInputStream(data->inStream, offset, SEEK_SET) < 0) {
		return OggFLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;
	}

	return OggFLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK;
}

static OggFLAC__SeekableStreamDecoderTellStatus of_tell_cb(const
							   OggFLAC__SeekableStreamDecoder
							   * decoder,
							   FLAC__uint64 *
							   offset, void *fdata)
{
	FlacData *data = (FlacData *) fdata;

	*offset = (long)(data->inStream->offset);

	return OggFLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK;
}

static OggFLAC__SeekableStreamDecoderLengthStatus of_length_cb(const
							       OggFLAC__SeekableStreamDecoder
							       * decoder,
							       FLAC__uint64 *
							       length,
							       void *fdata)
{
	FlacData *data = (FlacData *) fdata;

	*length = (size_t) (data->inStream->size);

	return OggFLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool of_EOF_cb(const OggFLAC__SeekableStreamDecoder * decoder,
			    void *fdata)
{
	FlacData *data = (FlacData *) fdata;

	if (inputStreamAtEOF(data->inStream) == 1)
		return true;
	return false;
}

static void of_error_cb(const OggFLAC__SeekableStreamDecoder * decoder,
			FLAC__StreamDecoderErrorStatus status, void *fdata)
{
	flac_error_common_cb("oggflac", status, (FlacData *) fdata);
}

static void oggflacPrintErroredState(OggFLAC__SeekableStreamDecoderState state)
{
	switch (state) {
	case OggFLAC__SEEKABLE_STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
		ERROR("oggflac allocation error\n");
		break;
	case OggFLAC__SEEKABLE_STREAM_DECODER_READ_ERROR:
		ERROR("oggflac read error\n");
		break;
	case OggFLAC__SEEKABLE_STREAM_DECODER_SEEK_ERROR:
		ERROR("oggflac seek error\n");
		break;
	case OggFLAC__SEEKABLE_STREAM_DECODER_STREAM_DECODER_ERROR:
		ERROR("oggflac seekable stream error\n");
		break;
	case OggFLAC__SEEKABLE_STREAM_DECODER_ALREADY_INITIALIZED:
		ERROR("oggflac decoder already initialized\n");
		break;
	case OggFLAC__SEEKABLE_STREAM_DECODER_INVALID_CALLBACK:
		ERROR("invalid oggflac callback\n");
		break;
	case OggFLAC__SEEKABLE_STREAM_DECODER_UNINITIALIZED:
		ERROR("oggflac decoder uninitialized\n");
		break;
	case OggFLAC__SEEKABLE_STREAM_DECODER_OK:
	case OggFLAC__SEEKABLE_STREAM_DECODER_SEEKING:
	case OggFLAC__SEEKABLE_STREAM_DECODER_END_OF_STREAM:
		break;
	}
}

static FLAC__StreamDecoderWriteStatus oggflacWrite(const
						   OggFLAC__SeekableStreamDecoder
						   * decoder,
						   const FLAC__Frame * frame,
						   const FLAC__int32 *
						   const buf[], void *vdata)
{
	FlacData *data = (FlacData *) vdata;
	FLAC__uint32 samples = frame->header.blocksize;
	FLAC__uint16 u16;
	unsigned char *uc;
	int c_samp, c_chan, d_samp;
	int i;
	float timeChange;

	timeChange = ((float)samples) / frame->header.sample_rate;
	data->time += timeChange;

	/* ogg123 uses a complicated method of calculating bitrate
	 * with averaging which I'm not too fond of.
	 * (waste of memory/CPU cycles, especially given this is _lossless_)
	 * a get_decode_position() is not available in OggFLAC, either
	 *
	 * this does not give an accurate bitrate:
	 * (bytes_last_read was set in the read callback)
	 data->bitRate = ((8.0 * data->bytes_last_read *
	 frame->header.sample_rate)
	 /((float)samples * 1000)) + 0.5;
	 */

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

/* used by TagDup */
static void of_metadata_dup_cb(const OggFLAC__SeekableStreamDecoder * decoder,
			       const FLAC__StreamMetadata * block, void *vdata)
{
	FlacData *data = (FlacData *) vdata;

	switch (block->type) {
	case FLAC__METADATA_TYPE_STREAMINFO:
		if (!data->tag)
			data->tag = newMpdTag();
		data->tag->time = ((float)block->data.stream_info.
				   total_samples) /
		    block->data.stream_info.sample_rate + 0.5;
		return;
	case FLAC__METADATA_TYPE_VORBIS_COMMENT:
		copyVorbisCommentBlockToMpdTag(block, data->tag);
	default:
		break;
	}
}

/* used by decode */
static void of_metadata_decode_cb(const OggFLAC__SeekableStreamDecoder * dec,
				  const FLAC__StreamMetadata * block,
				  void *vdata)
{
	flac_metadata_common_cb(block, (FlacData *) vdata);
}

static OggFLAC__SeekableStreamDecoder
    * full_decoder_init_and_read_metadata(FlacData * data,
					  unsigned int metadata_only)
{
	OggFLAC__SeekableStreamDecoder *decoder = NULL;
	unsigned int s = 1;

	if (!(decoder = OggFLAC__seekable_stream_decoder_new()))
		return NULL;

	if (metadata_only) {
		s &= OggFLAC__seekable_stream_decoder_set_metadata_callback
		    (decoder, of_metadata_dup_cb);
		s &= OggFLAC__seekable_stream_decoder_set_metadata_respond
		    (decoder, FLAC__METADATA_TYPE_STREAMINFO);
	} else {
		s &= OggFLAC__seekable_stream_decoder_set_metadata_callback
		    (decoder, of_metadata_decode_cb);
	}

	s &= OggFLAC__seekable_stream_decoder_set_read_callback(decoder,
								of_read_cb);
	s &= OggFLAC__seekable_stream_decoder_set_seek_callback(decoder,
								of_seek_cb);
	s &= OggFLAC__seekable_stream_decoder_set_tell_callback(decoder,
								of_tell_cb);
	s &= OggFLAC__seekable_stream_decoder_set_length_callback(decoder,
								  of_length_cb);
	s &= OggFLAC__seekable_stream_decoder_set_eof_callback(decoder,
							       of_EOF_cb);
	s &= OggFLAC__seekable_stream_decoder_set_write_callback(decoder,
								 oggflacWrite);
	s &= OggFLAC__seekable_stream_decoder_set_metadata_respond(decoder,
								   FLAC__METADATA_TYPE_VORBIS_COMMENT);
	s &= OggFLAC__seekable_stream_decoder_set_error_callback(decoder,
								 of_error_cb);
	s &= OggFLAC__seekable_stream_decoder_set_client_data(decoder,
							      (void *)data);

	if (!s) {
		ERROR("oggflac problem before init()\n");
		goto fail;
	}
	if (OggFLAC__seekable_stream_decoder_init(decoder) !=
	    OggFLAC__SEEKABLE_STREAM_DECODER_OK) {
		ERROR("oggflac problem doing init()\n");
		goto fail;
	}
	if (!OggFLAC__seekable_stream_decoder_process_until_end_of_metadata
	    (decoder)) {
		ERROR("oggflac problem reading metadata\n");
		goto fail;
	}

	return decoder;

fail:
	oggflacPrintErroredState(OggFLAC__seekable_stream_decoder_get_state
				 (decoder));
	OggFLAC__seekable_stream_decoder_delete(decoder);
	return NULL;
}

/* public functions: */
static MpdTag *oggflac_TagDup(char *file)
{
	InputStream inStream;
	OggFLAC__SeekableStreamDecoder *decoder;
	FlacData data;

	if (openInputStream(&inStream, file) < 0)
		return NULL;
	if (ogg_stream_type_detect(&inStream) != FLAC) {
		closeInputStream(&inStream);
		return NULL;
	}

	init_FlacData(&data, NULL, NULL, &inStream);

	/* errors here won't matter,
	 * data.tag will be set or unset, that's all we care about */
	decoder = full_decoder_init_and_read_metadata(&data, 1);

	oggflac_cleanup(&inStream, &data, decoder);

	return data.tag;
}

static unsigned int oggflac_try_decode(InputStream * inStream)
{
	return (ogg_stream_type_detect(inStream) == FLAC) ? 1 : 0;
}

static int oggflac_decode(OutputBuffer * cb, DecoderControl * dc,
			  InputStream * inStream)
{
	OggFLAC__SeekableStreamDecoder *decoder = NULL;
	FlacData data;
	int ret = 0;

	init_FlacData(&data, cb, dc, inStream);

	if (!(decoder = full_decoder_init_and_read_metadata(&data, 0))) {
		ret = -1;
		goto fail;
	}

	dc->state = DECODE_STATE_DECODE;

	while (1) {
		OggFLAC__seekable_stream_decoder_process_single(decoder);
		if (OggFLAC__seekable_stream_decoder_get_state(decoder) !=
		    OggFLAC__SEEKABLE_STREAM_DECODER_OK) {
			break;
		}
		if (dc->seek) {
			FLAC__uint64 sampleToSeek = dc->seekWhere *
			    dc->audioFormat.sampleRate + 0.5;
			if (OggFLAC__seekable_stream_decoder_seek_absolute
			    (decoder, sampleToSeek)) {
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
		oggflacPrintErroredState
		    (OggFLAC__seekable_stream_decoder_get_state(decoder));
		OggFLAC__seekable_stream_decoder_finish(decoder);
	}
	/* send last little bit */
	if (data.chunk_length > 0 && !dc->stop) {
		flacSendChunk(&data);
		flushOutputBuffer(data.cb);
	}

	dc->state = DECODE_STATE_STOP;
	dc->stop = 0;

fail:
	oggflac_cleanup(inStream, &data, decoder);

	return ret;
}

static char *oggflac_Suffixes[] = { "ogg", "oga", NULL };
static char *oggflac_mime_types[] = { "audio/x-flac+ogg",
                                      "application/ogg",
                                      "application/x-ogg",
                                      NULL };

InputPlugin oggflacPlugin = {
	"oggflac",
	NULL,
	NULL,
	oggflac_try_decode,
	oggflac_decode,
	NULL,
	oggflac_TagDup,
	INPUT_PLUGIN_STREAM_URL | INPUT_PLUGIN_STREAM_FILE,
	oggflac_Suffixes,
	oggflac_mime_types
};

#else /* !HAVE_FLAC */

InputPlugin oggflacPlugin;

#endif /* HAVE_OGGFLAC */
