/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
 * http://www.musicpd.org
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * OggFLAC support (half-stolen from flac_plugin.c :))
 */

#include "config.h" /* must be first for large file support */
#include "_flac_common.h"
#include "_ogg_common.h"
#include "flac_metadata.h"

#include <glib.h>
#include <OggFLAC/seekable_stream_decoder.h>
#include <assert.h>
#include <unistd.h>

static void oggflac_cleanup(OggFLAC__SeekableStreamDecoder * decoder)
{
	if (decoder)
		OggFLAC__seekable_stream_decoder_delete(decoder);
}

static OggFLAC__SeekableStreamDecoderReadStatus of_read_cb(G_GNUC_UNUSED const
							   OggFLAC__SeekableStreamDecoder
							   * decoder,
							   FLAC__byte buf[],
							   unsigned *bytes,
							   void *fdata)
{
	struct flac_data *data = (struct flac_data *) fdata;
	size_t r;

	r = decoder_read(data->decoder, data->input_stream,
			 (void *)buf, *bytes);
	*bytes = r;

	if (r == 0 && !input_stream_eof(data->input_stream) &&
	    decoder_get_command(data->decoder) == DECODE_COMMAND_NONE)
		return OggFLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR;

	return OggFLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK;
}

static OggFLAC__SeekableStreamDecoderSeekStatus of_seek_cb(G_GNUC_UNUSED const
							   OggFLAC__SeekableStreamDecoder
							   * decoder,
							   FLAC__uint64 offset,
							   void *fdata)
{
	struct flac_data *data = (struct flac_data *) fdata;

	if (!input_stream_seek(data->input_stream, offset, SEEK_SET))
		return OggFLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;

	return OggFLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK;
}

static OggFLAC__SeekableStreamDecoderTellStatus of_tell_cb(G_GNUC_UNUSED const
							   OggFLAC__SeekableStreamDecoder
							   * decoder,
							   FLAC__uint64 *
							   offset, void *fdata)
{
	struct flac_data *data = (struct flac_data *) fdata;

	*offset = (long)(data->input_stream->offset);

	return OggFLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK;
}

static OggFLAC__SeekableStreamDecoderLengthStatus of_length_cb(G_GNUC_UNUSED const
							       OggFLAC__SeekableStreamDecoder
							       * decoder,
							       FLAC__uint64 *
							       length,
							       void *fdata)
{
	struct flac_data *data = (struct flac_data *) fdata;

	if (data->input_stream->size < 0)
		return OggFLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR;

	*length = (size_t) (data->input_stream->size);

	return OggFLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool of_EOF_cb(G_GNUC_UNUSED const OggFLAC__SeekableStreamDecoder * decoder,
			    void *fdata)
{
	struct flac_data *data = (struct flac_data *) fdata;

	return (decoder_get_command(data->decoder) != DECODE_COMMAND_NONE &&
		decoder_get_command(data->decoder) != DECODE_COMMAND_SEEK) ||
		input_stream_eof(data->input_stream);
}

static void of_error_cb(G_GNUC_UNUSED const OggFLAC__SeekableStreamDecoder * decoder,
			FLAC__StreamDecoderErrorStatus status, void *fdata)
{
	flac_error_common_cb("oggflac", status, (struct flac_data *) fdata);
}

static void oggflacPrintErroredState(OggFLAC__SeekableStreamDecoderState state)
{
	switch (state) {
	case OggFLAC__SEEKABLE_STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
		g_warning("oggflac allocation error\n");
		break;
	case OggFLAC__SEEKABLE_STREAM_DECODER_READ_ERROR:
		g_warning("oggflac read error\n");
		break;
	case OggFLAC__SEEKABLE_STREAM_DECODER_SEEK_ERROR:
		g_warning("oggflac seek error\n");
		break;
	case OggFLAC__SEEKABLE_STREAM_DECODER_STREAM_DECODER_ERROR:
		g_warning("oggflac seekable stream error\n");
		break;
	case OggFLAC__SEEKABLE_STREAM_DECODER_ALREADY_INITIALIZED:
		g_warning("oggflac decoder already initialized\n");
		break;
	case OggFLAC__SEEKABLE_STREAM_DECODER_INVALID_CALLBACK:
		g_warning("invalid oggflac callback\n");
		break;
	case OggFLAC__SEEKABLE_STREAM_DECODER_UNINITIALIZED:
		g_warning("oggflac decoder uninitialized\n");
		break;
	case OggFLAC__SEEKABLE_STREAM_DECODER_OK:
	case OggFLAC__SEEKABLE_STREAM_DECODER_SEEKING:
	case OggFLAC__SEEKABLE_STREAM_DECODER_END_OF_STREAM:
		break;
	}
}

static FLAC__StreamDecoderWriteStatus
oggflac_write_cb(G_GNUC_UNUSED const OggFLAC__SeekableStreamDecoder *decoder,
		 const FLAC__Frame *frame, const FLAC__int32 *const buf[],
		 void *vdata)
{
	struct flac_data *data = (struct flac_data *) vdata;

	return flac_common_write(data, frame, buf, 0);
}

/* used by TagDup */
static void of_metadata_dup_cb(G_GNUC_UNUSED const OggFLAC__SeekableStreamDecoder * decoder,
			       const FLAC__StreamMetadata * block, void *vdata)
{
	struct flac_data *data = (struct flac_data *) vdata;

	assert(data->tag != NULL);

	flac_tag_apply_metadata(data->tag, NULL, block);
}

/* used by decode */
static void of_metadata_decode_cb(G_GNUC_UNUSED const OggFLAC__SeekableStreamDecoder * dec,
				  const FLAC__StreamMetadata * block,
				  void *vdata)
{
	flac_metadata_common_cb(block, (struct flac_data *) vdata);
}

static OggFLAC__SeekableStreamDecoder *
full_decoder_init_and_read_metadata(struct flac_data *data,
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
								 oggflac_write_cb);
	s &= OggFLAC__seekable_stream_decoder_set_metadata_respond(decoder,
								   FLAC__METADATA_TYPE_VORBIS_COMMENT);
	s &= OggFLAC__seekable_stream_decoder_set_error_callback(decoder,
								 of_error_cb);
	s &= OggFLAC__seekable_stream_decoder_set_client_data(decoder,
							      (void *)data);

	if (!s) {
		g_warning("oggflac problem before init()\n");
		goto fail;
	}
	if (OggFLAC__seekable_stream_decoder_init(decoder) !=
	    OggFLAC__SEEKABLE_STREAM_DECODER_OK) {
		g_warning("oggflac problem doing init()\n");
		goto fail;
	}
	if (!OggFLAC__seekable_stream_decoder_process_until_end_of_metadata
	    (decoder)) {
		g_warning("oggflac problem reading metadata\n");
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
static struct tag *
oggflac_tag_dup(const char *file)
{
	struct input_stream input_stream;
	OggFLAC__SeekableStreamDecoder *decoder;
	struct flac_data data;
	struct tag *tag;

	if (!input_stream_open(&input_stream, file))
		return NULL;
	if (ogg_stream_type_detect(&input_stream) != FLAC) {
		input_stream_close(&input_stream);
		return NULL;
	}

	/* rewind the stream, because ogg_stream_type_detect() has
	   moved it */
	input_stream_seek(&input_stream, 0, SEEK_SET);

	flac_data_init(&data, NULL, &input_stream);

	data.tag = tag_new();

	/* errors here won't matter,
	 * data.tag will be set or unset, that's all we care about */
	decoder = full_decoder_init_and_read_metadata(&data, 1);

	oggflac_cleanup(decoder);
	input_stream_close(&input_stream);

	if (tag_is_defined(data.tag)) {
		tag = data.tag;
		data.tag = NULL;
	} else
		tag = NULL;

	flac_data_deinit(&data);

	return tag;
}

static void
oggflac_decode(struct decoder * mpd_decoder, struct input_stream *input_stream)
{
	OggFLAC__SeekableStreamDecoder *decoder = NULL;
	struct flac_data data;
	struct audio_format audio_format;

	if (ogg_stream_type_detect(input_stream) != FLAC)
		return;

	/* rewind the stream, because ogg_stream_type_detect() has
	   moved it */
	input_stream_seek(input_stream, 0, SEEK_SET);

	flac_data_init(&data, mpd_decoder, input_stream);

	if (!(decoder = full_decoder_init_and_read_metadata(&data, 0))) {
		goto fail;
	}

	if (!flac_data_get_audio_format(&data, &audio_format))
		goto fail;

	decoder_initialized(mpd_decoder, &audio_format,
			    input_stream->seekable,
			    (float)data.stream_info.total_samples /
			    (float)data.stream_info.sample_rate);

	while (true) {
		OggFLAC__seekable_stream_decoder_process_single(decoder);
		if (OggFLAC__seekable_stream_decoder_get_state(decoder) !=
		    OggFLAC__SEEKABLE_STREAM_DECODER_OK) {
			break;
		}
		if (decoder_get_command(mpd_decoder) == DECODE_COMMAND_SEEK) {
			FLAC__uint64 seek_sample = decoder_seek_where(mpd_decoder) *
			    data.stream_info.sample_rate;
			if (OggFLAC__seekable_stream_decoder_seek_absolute
			    (decoder, seek_sample)) {
				data.next_frame = seek_sample;
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
	oggflac_cleanup(decoder);
	flac_data_deinit(&data);
}

static const char *const oggflac_suffixes[] = { "ogg", "oga", NULL };
static const char *const oggflac_mime_types[] = {
	"audio/x-flac+ogg",
	"application/ogg",
	"application/x-ogg",
	NULL
};

const struct decoder_plugin oggflac_decoder_plugin = {
	.name = "oggflac",
	.stream_decode = oggflac_decode,
	.tag_dup = oggflac_tag_dup,
	.suffixes = oggflac_suffixes,
	.mime_types = oggflac_mime_types
};
