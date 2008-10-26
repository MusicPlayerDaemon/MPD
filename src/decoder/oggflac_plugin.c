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
#include "_ogg_common.h"

#include "../utils.h"
#include "../log.h"

#include <OggFLAC/seekable_stream_decoder.h>

static void oggflac_cleanup(FlacData * data,
			    OggFLAC__SeekableStreamDecoder * decoder)
{
	if (data->replayGainInfo)
		freeReplayGainInfo(data->replayGainInfo);
	if (decoder)
		OggFLAC__seekable_stream_decoder_delete(decoder);
}

static OggFLAC__SeekableStreamDecoderReadStatus of_read_cb(mpd_unused const
							   OggFLAC__SeekableStreamDecoder
							   * decoder,
							   FLAC__byte buf[],
							   unsigned *bytes,
							   void *fdata)
{
	FlacData *data = (FlacData *) fdata;
	size_t r;

	r = decoder_read(data->decoder, data->inStream, (void *)buf, *bytes);
	*bytes = r;

	if (r == 0 && !input_stream_eof(data->inStream) &&
	    decoder_get_command(data->decoder) == DECODE_COMMAND_NONE)
		return OggFLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR;

	return OggFLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK;
}

static OggFLAC__SeekableStreamDecoderSeekStatus of_seek_cb(mpd_unused const
							   OggFLAC__SeekableStreamDecoder
							   * decoder,
							   FLAC__uint64 offset,
							   void *fdata)
{
	FlacData *data = (FlacData *) fdata;

	if (input_stream_seek(data->inStream, offset, SEEK_SET) < 0) {
		return OggFLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;
	}

	return OggFLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK;
}

static OggFLAC__SeekableStreamDecoderTellStatus of_tell_cb(mpd_unused const
							   OggFLAC__SeekableStreamDecoder
							   * decoder,
							   FLAC__uint64 *
							   offset, void *fdata)
{
	FlacData *data = (FlacData *) fdata;

	*offset = (long)(data->inStream->offset);

	return OggFLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK;
}

static OggFLAC__SeekableStreamDecoderLengthStatus of_length_cb(mpd_unused const
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

static FLAC__bool of_EOF_cb(mpd_unused const OggFLAC__SeekableStreamDecoder * decoder,
			    void *fdata)
{
	FlacData *data = (FlacData *) fdata;

	return (decoder_get_command(data->decoder) != DECODE_COMMAND_NONE &&
		decoder_get_command(data->decoder) != DECODE_COMMAND_SEEK) ||
		input_stream_eof(data->inStream);
}

static void of_error_cb(mpd_unused const OggFLAC__SeekableStreamDecoder * decoder,
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

static FLAC__StreamDecoderWriteStatus oggflacWrite(mpd_unused const
						   OggFLAC__SeekableStreamDecoder
						   * decoder,
						   const FLAC__Frame * frame,
						   const FLAC__int32 *
						   const buf[], void *vdata)
{
	FlacData *data = (FlacData *) vdata;
	FLAC__uint32 samples = frame->header.blocksize;
	float timeChange;

	timeChange = ((float)samples) / frame->header.sample_rate;
	data->time += timeChange;

	return flac_common_write(data, frame, buf);
}

/* used by TagDup */
static void of_metadata_dup_cb(mpd_unused const OggFLAC__SeekableStreamDecoder * decoder,
			       const FLAC__StreamMetadata * block, void *vdata)
{
	FlacData *data = (FlacData *) vdata;

	switch (block->type) {
	case FLAC__METADATA_TYPE_STREAMINFO:
		if (!data->tag)
			data->tag = tag_new();
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
static void of_metadata_decode_cb(mpd_unused const OggFLAC__SeekableStreamDecoder * dec,
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
static struct tag *oggflac_TagDup(char *file)
{
	InputStream inStream;
	OggFLAC__SeekableStreamDecoder *decoder;
	FlacData data;

	if (input_stream_open(&inStream, file) < 0)
		return NULL;
	if (ogg_stream_type_detect(&inStream) != FLAC) {
		input_stream_close(&inStream);
		return NULL;
	}

	init_FlacData(&data, NULL, &inStream);

	/* errors here won't matter,
	 * data.tag will be set or unset, that's all we care about */
	decoder = full_decoder_init_and_read_metadata(&data, 1);

	oggflac_cleanup(&data, decoder);
	input_stream_close(&inStream);

	return data.tag;
}

static bool oggflac_try_decode(InputStream * inStream)
{
	if (!inStream->seekable)
		/* we cannot seek after the detection, so don't bother
		   checking */
		return true;

	return ogg_stream_type_detect(inStream) == FLAC;
}

static int oggflac_decode(struct decoder * mpd_decoder, InputStream * inStream)
{
	OggFLAC__SeekableStreamDecoder *decoder = NULL;
	FlacData data;
	int ret = 0;

	init_FlacData(&data, mpd_decoder, inStream);

	if (!(decoder = full_decoder_init_and_read_metadata(&data, 0))) {
		ret = -1;
		goto fail;
	}

	decoder_initialized(mpd_decoder, &data.audio_format, data.total_time);

	while (1) {
		OggFLAC__seekable_stream_decoder_process_single(decoder);
		if (OggFLAC__seekable_stream_decoder_get_state(decoder) !=
		    OggFLAC__SEEKABLE_STREAM_DECODER_OK) {
			break;
		}
		if (decoder_get_command(mpd_decoder) == DECODE_COMMAND_SEEK) {
			FLAC__uint64 sampleToSeek = decoder_seek_where(mpd_decoder) *
			    data.audio_format.sample_rate + 0.5;
			if (OggFLAC__seekable_stream_decoder_seek_absolute
			    (decoder, sampleToSeek)) {
				decoder_clear(mpd_decoder);
				data.time = ((float)sampleToSeek) /
				    data.audio_format.sample_rate;
				data.position = 0;
				decoder_command_finished(mpd_decoder);
			} else
				decoder_seek_error(mpd_decoder);
		}
	}

	if (decoder_get_command(mpd_decoder) == DECODE_COMMAND_NONE) {
		oggflacPrintErroredState
		    (OggFLAC__seekable_stream_decoder_get_state(decoder));
		OggFLAC__seekable_stream_decoder_finish(decoder);
	}

fail:
	oggflac_cleanup(&data, decoder);

	return ret;
}

static const char *oggflac_Suffixes[] = { "ogg", "oga",NULL };
static const char *oggflac_mime_types[] = { "audio/x-flac+ogg",
					    "application/ogg",
					    "application/x-ogg",
					    NULL };

struct decoder_plugin oggflacPlugin = {
	.name = "oggflac",
	.try_decode = oggflac_try_decode,
	.stream_decode = oggflac_decode,
	.tag_dup = oggflac_TagDup,
	.stream_types = INPUT_PLUGIN_STREAM_URL | INPUT_PLUGIN_STREAM_FILE,
	.suffixes = oggflac_Suffixes,
	.mime_types = oggflac_mime_types
};
