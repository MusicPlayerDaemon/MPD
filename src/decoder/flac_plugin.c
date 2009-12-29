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

#include "_flac_common.h"

#include <glib.h>

#include <assert.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#ifdef HAVE_CUE /* libcue */
#include "../cue/cue_tag.h"
#endif

/* this code was based on flac123, from flac-tools */

static flac_read_status
flac_read_cb(G_GNUC_UNUSED const flac_decoder *fd,
	     FLAC__byte buf[], flac_read_status_size_t *bytes,
	     void *fdata)
{
	struct flac_data *data = fdata;
	size_t r;

	r = decoder_read(data->decoder, data->input_stream,
			 (void *)buf, *bytes);
	*bytes = r;

	if (r == 0) {
		if (decoder_get_command(data->decoder) != DECODE_COMMAND_NONE ||
		    input_stream_eof(data->input_stream))
			return flac_read_status_eof;
		else
			return flac_read_status_abort;
	}

	return flac_read_status_continue;
}

static flac_seek_status
flac_seek_cb(G_GNUC_UNUSED const flac_decoder *fd,
	     FLAC__uint64 offset, void *fdata)
{
	struct flac_data *data = (struct flac_data *) fdata;

	if (!input_stream_seek(data->input_stream, offset, SEEK_SET))
		return flac_seek_status_error;

	return flac_seek_status_ok;
}

static flac_tell_status
flac_tell_cb(G_GNUC_UNUSED const flac_decoder *fd,
	     FLAC__uint64 * offset, void *fdata)
{
	struct flac_data *data = (struct flac_data *) fdata;

	*offset = (long)(data->input_stream->offset);

	return flac_tell_status_ok;
}

static flac_length_status
flac_length_cb(G_GNUC_UNUSED const flac_decoder *fd,
	       FLAC__uint64 * length, void *fdata)
{
	struct flac_data *data = (struct flac_data *) fdata;

	if (data->input_stream->size < 0)
		return flac_length_status_unsupported;

	*length = (size_t) (data->input_stream->size);

	return flac_length_status_ok;
}

static FLAC__bool
flac_eof_cb(G_GNUC_UNUSED const flac_decoder *fd, void *fdata)
{
	struct flac_data *data = (struct flac_data *) fdata;

	return (decoder_get_command(data->decoder) != DECODE_COMMAND_NONE &&
		decoder_get_command(data->decoder) != DECODE_COMMAND_SEEK) ||
		input_stream_eof(data->input_stream);
}

static void
flac_error_cb(G_GNUC_UNUSED const flac_decoder *fd,
	      FLAC__StreamDecoderErrorStatus status, void *fdata)
{
	flac_error_common_cb("flac", status, (struct flac_data *) fdata);
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

	g_warning("%s\n", str);
}

static bool
flac_init(FLAC__SeekableStreamDecoder *dec,
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
	return FLAC__seekable_stream_decoder_set_read_callback(dec, read_cb) &&
		FLAC__seekable_stream_decoder_set_seek_callback(dec, seek_cb) &&
		FLAC__seekable_stream_decoder_set_tell_callback(dec, tell_cb) &&
		FLAC__seekable_stream_decoder_set_length_callback(dec, length_cb) &&
		FLAC__seekable_stream_decoder_set_eof_callback(dec, eof_cb) &&
		FLAC__seekable_stream_decoder_set_write_callback(dec, write_cb) &&
		FLAC__seekable_stream_decoder_set_metadata_callback(dec, metadata_cb) &&
		FLAC__seekable_stream_decoder_set_metadata_respond(dec, FLAC__METADATA_TYPE_VORBIS_COMMENT) &&
		FLAC__seekable_stream_decoder_set_error_callback(dec, error_cb) &&
		FLAC__seekable_stream_decoder_set_client_data(dec, data) &&
		FLAC__seekable_stream_decoder_init(dec) == FLAC__SEEKABLE_STREAM_DECODER_OK;
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

	g_warning("%s\n", str);
}
#endif /* FLAC_API_VERSION_CURRENT >= 7 */

static void flacMetadata(G_GNUC_UNUSED const flac_decoder * dec,
			 const FLAC__StreamMetadata * block, void *vdata)
{
	flac_metadata_common_cb(block, (struct flac_data *) vdata);
}

static FLAC__StreamDecoderWriteStatus
flac_write_cb(const flac_decoder *dec, const FLAC__Frame *frame,
	      const FLAC__int32 *const buf[], void *vdata)
{
	FLAC__uint32 samples = frame->header.blocksize;
	struct flac_data *data = (struct flac_data *) vdata;
	float timeChange;
	FLAC__uint64 newPosition = 0;

	timeChange = ((float)samples) / frame->header.sample_rate;
	data->time += timeChange;

	flac_get_decode_position(dec, &newPosition);
	if (data->position && newPosition >= data->position) {
		assert(timeChange >= 0);

		data->bit_rate =
		    ((newPosition - data->position) * 8.0 / timeChange)
		    / 1000 + 0.5;
	}
	data->position = newPosition;

	return flac_common_write(data, frame, buf);
}

static struct tag *
flac_tag_load(const char *file, const char *char_tnum)
{
	struct tag *tag;
	FLAC__Metadata_SimpleIterator *it;
	FLAC__StreamMetadata *block = NULL;

	it = FLAC__metadata_simple_iterator_new();
	if (!FLAC__metadata_simple_iterator_init(it, file, 1, 0)) {
		const char *err;
		FLAC_API FLAC__Metadata_SimpleIteratorStatus s;

		s = FLAC__metadata_simple_iterator_status(it);

		switch (s) { /* slightly more human-friendly messages: */
		case FLAC__METADATA_SIMPLE_ITERATOR_STATUS_ILLEGAL_INPUT:
			err = "illegal input";
			break;
		case FLAC__METADATA_SIMPLE_ITERATOR_STATUS_ERROR_OPENING_FILE:
			err = "error opening file";
			break;
		case FLAC__METADATA_SIMPLE_ITERATOR_STATUS_NOT_A_FLAC_FILE:
			err = "not a FLAC file";
			break;
		default:
			err = FLAC__Metadata_SimpleIteratorStatusString[s];
		}
		g_debug("Reading '%s' metadata gave the following error: %s\n",
			file, err);
		FLAC__metadata_simple_iterator_delete(it);
		return NULL;
	}

	tag = tag_new();
	do {
		block = FLAC__metadata_simple_iterator_get_block(it);
		if (!block)
			break;
		if (block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
			flac_vorbis_comments_to_tag(tag, char_tnum, block);
		} else if (block->type == FLAC__METADATA_TYPE_STREAMINFO) {
			tag->time = ((float)block->data.stream_info.total_samples) /
			    block->data.stream_info.sample_rate + 0.5;
		}
		FLAC__metadata_object_delete(block);
	} while (FLAC__metadata_simple_iterator_next(it));

	FLAC__metadata_simple_iterator_delete(it);

	if (!tag_is_defined(tag)) {
		tag_free(tag);
		tag = NULL;
	}

	return tag;
}

#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT > 7

static struct tag *
flac_cue_tag_load(const char *file)
{
	struct tag* tag = NULL;
	char* char_tnum = NULL;
	char* ptr = NULL;
	unsigned int tnum = 0;
	unsigned int sample_rate = 0;
	FLAC__uint64 track_time = 0;
#ifdef HAVE_CUE /* libcue */
	FLAC__StreamMetadata* vc;
#endif /* libcue */
	FLAC__StreamMetadata* si = FLAC__metadata_object_new(FLAC__METADATA_TYPE_STREAMINFO);
	FLAC__StreamMetadata* cs;

	tnum = flac_vtrack_tnum(file);
	char_tnum = g_strdup_printf("%u", tnum);

	ptr = strrchr(file, '/');
	*ptr = '\0';

#ifdef HAVE_CUE /* libcue */
	if (FLAC__metadata_get_tags(file, &vc))
	{
		for (unsigned i = 0; i < vc->data.vorbis_comment.num_comments;
		     i++)
		{
			if ((ptr = (char*)vc->data.vorbis_comment.comments[i].entry) != NULL)
			{
				if (g_ascii_strncasecmp(ptr, "cuesheet", 8) == 0)
				{
					while (*(++ptr) != '=');
					tag = cue_tag_string(   ++ptr,
								tnum);
				}
			}
		}

		FLAC__metadata_object_delete(vc);
	}
#endif /* libcue */

	if (tag == NULL)
		tag = flac_tag_load(file, char_tnum);

	if (char_tnum != NULL)
	{
		tag_add_item(	tag,
				TAG_ITEM_TRACK,
				char_tnum);
		g_free(char_tnum);
	}

	if (FLAC__metadata_get_streaminfo(file, si))
	{
		sample_rate = si->data.stream_info.sample_rate;
		FLAC__metadata_object_delete(si);
	}

	if (FLAC__metadata_get_cuesheet(file, &cs))
	{
		if (cs->data.cue_sheet.tracks != NULL
				&& (tnum <= cs->data.cue_sheet.num_tracks - 1))
		{
			track_time = cs->data.cue_sheet.tracks[tnum].offset
				- cs->data.cue_sheet.tracks[tnum - 1].offset;
		}
		FLAC__metadata_object_delete(cs);
	}

	if (sample_rate != 0)
	{
		tag->time = (unsigned int)(track_time/sample_rate);
	}

	return tag;
}

#endif /* FLAC_API_VERSION_CURRENT >= 7 */

static struct tag *
flac_tag_dup(const char *file)
{
#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT > 7
	struct stat st;

	if (stat(file, &st) < 0)
		return flac_cue_tag_load(file);
	else
#endif /* FLAC_API_VERSION_CURRENT >= 7 */
		return flac_tag_load(file, NULL);
}

static void
flac_decode_internal(struct decoder * decoder,
		     struct input_stream *input_stream,
		     bool is_ogg)
{
	flac_decoder *flac_dec;
	struct flac_data data;
	enum decoder_command cmd;
	const char *err = NULL;

	if (!(flac_dec = flac_new()))
		return;
	flac_data_init(&data, decoder, input_stream);
	data.tag = tag_new();

#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT > 7
        if(!FLAC__stream_decoder_set_metadata_respond(flac_dec, FLAC__METADATA_TYPE_VORBIS_COMMENT))
        {
                g_debug("Failed to set metadata respond\n");
        }
#endif

	if (is_ogg) {
		if (!flac_ogg_init(flac_dec, flac_read_cb,
				   flac_seek_cb, flac_tell_cb,
				   flac_length_cb, flac_eof_cb,
				   flac_write_cb, flacMetadata,
				   flac_error_cb, (void *)&data)) {
			err = "doing Ogg init()";
			goto fail;
		}
	} else {
		if (!flac_init(flac_dec, flac_read_cb,
			       flac_seek_cb, flac_tell_cb,
			       flac_length_cb, flac_eof_cb,
			       flac_write_cb, flacMetadata,
			       flac_error_cb, (void *)&data)) {
			err = "doing init()";
			goto fail;
		}
	}

	if (!flac_process_metadata(flac_dec)) {
		err = "problem reading metadata";
		goto fail;
	}

	if (!audio_format_valid(&data.audio_format)) {
		g_warning("Invalid audio format: %u:%u:%u\n",
			  data.audio_format.sample_rate,
			  data.audio_format.bits,
			  data.audio_format.channels);
		goto fail;
	}

	decoder_initialized(decoder, &data.audio_format,
			    input_stream->seekable, data.total_time);

	while (true) {
		if (!tag_is_empty(data.tag)) {
			cmd = decoder_tag(decoder, input_stream, data.tag);
			tag_free(data.tag);
			data.tag = tag_new();
		} else
			cmd = decoder_get_command(decoder);

		if (cmd == DECODE_COMMAND_SEEK) {
			FLAC__uint64 seek_sample = decoder_seek_where(decoder) *
			    data.audio_format.sample_rate + 0.5;
			if (flac_seek_absolute(flac_dec, seek_sample)) {
				data.time = ((float)seek_sample) /
				    data.audio_format.sample_rate;
				data.position = 0;
				decoder_command_finished(decoder);
			} else
				decoder_seek_error(decoder);
		} else if (cmd == DECODE_COMMAND_STOP ||
			   flac_get_state(flac_dec) == flac_decoder_eof)
			break;

		if (!flac_process_single(flac_dec)) {
			cmd = decoder_get_command(decoder);
			if (cmd != DECODE_COMMAND_SEEK)
				break;
		}
	}
	if (cmd != DECODE_COMMAND_STOP) {
		flacPrintErroredState(flac_get_state(flac_dec));
		flac_finish(flac_dec);
	}

fail:
	if (data.replay_gain_info)
		replay_gain_info_free(data.replay_gain_info);

	tag_free(data.tag);

	if (flac_dec)
		flac_delete(flac_dec);

	if (err)
		g_warning("%s\n", err);
}

static void
flac_decode(struct decoder * decoder, struct input_stream *input_stream)
{
	flac_decode_internal(decoder, input_stream, false);
}

#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT > 7

/**
 * @brief Decode a flac file with embedded cue sheets
 * @param const char* fname filename on fs
 */
static void
flac_container_decode(struct decoder* decoder,
		     const char* fname,
		     bool is_ogg)
{
	unsigned int tnum = 0;
	FLAC__uint64 t_start = 0;
	FLAC__uint64 t_end = 0;
	FLAC__uint64 track_time = 0;
	FLAC__StreamMetadata* cs = NULL;

	flac_decoder *flac_dec;
	struct flac_data data;
	const char *err = NULL;

	char* pathname = g_strdup(fname);
	char* slash = strrchr(pathname, '/');
	*slash = '\0';

	tnum = flac_vtrack_tnum(fname);

	cs = FLAC__metadata_object_new(FLAC__METADATA_TYPE_CUESHEET);

	FLAC__metadata_get_cuesheet(pathname, &cs);

	if (cs != NULL)
	{
		if (cs->data.cue_sheet.tracks != NULL
				&& (tnum <= cs->data.cue_sheet.num_tracks - 1))
		{
			t_start = cs->data.cue_sheet.tracks[tnum - 1].offset;
			t_end = cs->data.cue_sheet.tracks[tnum].offset;
			track_time = cs->data.cue_sheet.tracks[tnum].offset
				- cs->data.cue_sheet.tracks[tnum - 1].offset;
		}

		FLAC__metadata_object_delete(cs);
	}
	else
	{
		g_free(pathname);
		return;
	}

	if (!(flac_dec = flac_new()))
	{
		g_free(pathname);
		return;
	}

	flac_data_init(&data, decoder, NULL);

#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT > 7
        if(!FLAC__stream_decoder_set_metadata_respond(flac_dec, FLAC__METADATA_TYPE_VORBIS_COMMENT))
        {
                g_debug("Failed to set metadata respond\n");
        }
#endif


	if (is_ogg)
	{
		if (FLAC__stream_decoder_init_ogg_file(	flac_dec,
							pathname,
							flac_write_cb,
							flacMetadata,
							flac_error_cb,
							(void*) &data	)
			!= FLAC__STREAM_DECODER_INIT_STATUS_OK		)
		{
			err = "doing Ogg init()";
			goto fail;
		}
	}
	else
	{
		if (FLAC__stream_decoder_init_file(	flac_dec,
							pathname,
							flac_write_cb,
							flacMetadata,
							flac_error_cb,
							(void*) &data	)
			!= FLAC__STREAM_DECODER_INIT_STATUS_OK		)
		{
			err = "doing init()";
			goto fail;
		}
	}

	if (!flac_process_metadata(flac_dec))
	{
		err = "problem reading metadata";
		goto fail;
	}

	if (!audio_format_valid(&data.audio_format))
	{
		g_warning("Invalid audio format: %u:%u:%u\n",
			  data.audio_format.sample_rate,
			  data.audio_format.bits,
			  data.audio_format.channels);
		goto fail;
	}

	// set track time (order is important: after stream init)
	data.total_time = ((float)track_time / (float)data.audio_format.sample_rate);
	data.position = 0;

	decoder_initialized(decoder, &data.audio_format,
			    true, data.total_time);

	// seek to song start (order is important: after decoder init)
	flac_seek_absolute(flac_dec, (FLAC__uint64)t_start);

	while (true)
	{
		if (!flac_process_single(flac_dec))
			break;

		// we only need to break at the end of track if we are in "cue mode"
		if (data.time >= data.total_time)
		{
			flacPrintErroredState(flac_get_state(flac_dec));
			flac_finish(flac_dec);
		}

		if (decoder_get_command(decoder) == DECODE_COMMAND_SEEK)
		{
			FLAC__uint64 seek_sample = t_start +
				(decoder_seek_where(decoder) * data.audio_format.sample_rate);

			if (seek_sample >= t_start && seek_sample <= t_end &&
			    flac_seek_absolute(flac_dec, (FLAC__uint64)seek_sample)) {
				data.time = (float)(seek_sample - t_start) /
					data.audio_format.sample_rate;
				data.position = 0;

				decoder_command_finished(decoder);
			} else
				decoder_seek_error(decoder);
		}
		else if (flac_get_state(flac_dec) == flac_decoder_eof)
			break;
	}

	if (decoder_get_command(decoder) != DECODE_COMMAND_STOP)
	{
		flacPrintErroredState(flac_get_state(flac_dec));
		flac_finish(flac_dec);
	}

fail:
	if (pathname)
		g_free(pathname);

	if (data.replay_gain_info)
		replay_gain_info_free(data.replay_gain_info);

	if (flac_dec)
		flac_delete(flac_dec);

	if (err)
		g_warning("%s\n", err);
}

/**
 * @brief Open a flac file for decoding
 * @param const char* fname filename on fs
 */
static void
flac_filedecode_internal(struct decoder* decoder,
		     const char* fname,
		     bool is_ogg)
{
	flac_decoder *flac_dec;
	struct flac_data data;
	const char *err = NULL;
	unsigned int flac_err_state = 0;

	if (!(flac_dec = flac_new()))
		return;

	flac_data_init(&data, decoder, NULL);

#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT > 7
        if(!FLAC__stream_decoder_set_metadata_respond(flac_dec, FLAC__METADATA_TYPE_VORBIS_COMMENT))
        {
                g_debug("Failed to set metadata respond\n");
        }
#endif


	if (is_ogg)
	{
		if (	(flac_err_state = FLAC__stream_decoder_init_ogg_file(	flac_dec,
										fname,
										flac_write_cb,
										flacMetadata,
										flac_error_cb,
										(void*) &data	))
			== FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE)
		{
			flac_container_decode(decoder, fname, is_ogg);
		}
		else if (flac_err_state != FLAC__STREAM_DECODER_INIT_STATUS_OK)
		{
			err = "doing Ogg init()";
			goto fail;
		}
	}
	else
	{
		if (	(flac_err_state = FLAC__stream_decoder_init_file(	flac_dec,
										fname,
										flac_write_cb,
										flacMetadata,
										flac_error_cb,
										(void*) &data	))
			== FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE)
		{
			flac_container_decode(decoder, fname, is_ogg);
		}
		else if (flac_err_state != FLAC__STREAM_DECODER_INIT_STATUS_OK)
		{
			err = "doing init()";
			goto fail;
		}
	}

	if (!flac_process_metadata(flac_dec))
	{
		err = "problem reading metadata";
		goto fail;
	}

	if (!audio_format_valid(&data.audio_format))
	{
		g_warning("Invalid audio format: %u:%u:%u\n",
			  data.audio_format.sample_rate,
			  data.audio_format.bits,
			  data.audio_format.channels);
		goto fail;
	}

	decoder_initialized(decoder, &data.audio_format,
			    true, data.total_time);

	while (true)
	{
		if (!flac_process_single(flac_dec))
			break;

		if (decoder_get_command(decoder) == DECODE_COMMAND_SEEK)
		{
			FLAC__uint64 seek_sample = decoder_seek_where(decoder) *
			    data.audio_format.sample_rate + 0.5;
			if (flac_seek_absolute(flac_dec, seek_sample))
			{
				data.time = ((float)seek_sample) /
				    data.audio_format.sample_rate;
				data.position = 0;
				decoder_command_finished(decoder);
			}
			else
				decoder_seek_error(decoder);

		}
		else if (flac_get_state(flac_dec) == flac_decoder_eof)
			break;
	}

	if (decoder_get_command(decoder) != DECODE_COMMAND_STOP)
	{
		flacPrintErroredState(flac_get_state(flac_dec));
		flac_finish(flac_dec);
	}

fail:
	if (data.replay_gain_info)
		replay_gain_info_free(data.replay_gain_info);

	if (flac_dec)
		flac_delete(flac_dec);

	if (err)
		g_warning("%s\n", err);
}

/**
 * @brief	wrapper function for
 * 		flac_filedecode_internal method
 * 		for decoding without ogg
 */
static void
flac_filedecode(struct decoder *decoder, const char *fname)
{
	struct stat st;

	if (stat(fname, &st) < 0) {
		flac_container_decode(decoder, fname, false);
	} else 
		flac_filedecode_internal(decoder, fname, false);
}

#endif /* FLAC_API_VERSION_CURRENT >= 7 */

#ifndef HAVE_OGGFLAC

static bool
oggflac_init(G_GNUC_UNUSED const struct config_param *param)
{
#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT > 7
	return !!FLAC_API_SUPPORTS_OGG_FLAC;
#else
	/* disable oggflac when libflac is too old */
	return false;
#endif
}

#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT > 7

static struct tag *
oggflac_tag_dup(const char *file)
{
	struct tag *ret = NULL;
	FLAC__Metadata_Iterator *it;
	FLAC__StreamMetadata *block;
	FLAC__Metadata_Chain *chain = FLAC__metadata_chain_new();

	if (!(FLAC__metadata_chain_read_ogg(chain, file)))
		goto out;
	it = FLAC__metadata_iterator_new();
	FLAC__metadata_iterator_init(it, chain);

	ret = tag_new();
	do {
		if (!(block = FLAC__metadata_iterator_get_block(it)))
			break;
		if (block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
			flac_vorbis_comments_to_tag(ret, NULL, block);
		} else if (block->type == FLAC__METADATA_TYPE_STREAMINFO) {
			ret->time = ((float)block->data.stream_info.
				     total_samples) /
			    block->data.stream_info.sample_rate + 0.5;
		}
	} while (FLAC__metadata_iterator_next(it));
	FLAC__metadata_iterator_delete(it);

	if (!tag_is_defined(ret)) {
		tag_free(ret);
		ret = NULL;
	}

out:
	FLAC__metadata_chain_delete(chain);
	return ret;
}

static void
oggflac_decode(struct decoder *decoder, struct input_stream *input_stream)
{
	if (ogg_stream_type_detect(input_stream) != FLAC)
		return;

	/* rewind the stream, because ogg_stream_type_detect() has
	   moved it */
	input_stream_seek(input_stream, 0, SEEK_SET);

	flac_decode_internal(decoder, input_stream, true);
}

static const char *const oggflac_suffixes[] = { "ogg", "oga", NULL };
static const char *const oggflac_mime_types[] = {
	"application/ogg",
	"application/x-ogg",
	"audio/ogg",
	"audio/x-flac+ogg",
	"audio/x-ogg",
	NULL
};

#endif /* FLAC_API_VERSION_CURRENT >= 7 */

const struct decoder_plugin oggflac_decoder_plugin = {
	.name = "oggflac",
	.init = oggflac_init,
#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT > 7
	.stream_decode = oggflac_decode,
	.tag_dup = oggflac_tag_dup,
	.suffixes = oggflac_suffixes,
	.mime_types = oggflac_mime_types
#endif
};

#endif /* HAVE_OGGFLAC */

static const char *const flac_suffixes[] = { "flac", NULL };
static const char *const flac_mime_types[] = {
	"application/flac",
	"application/x-flac",
	"audio/flac",
	"audio/x-flac",
	NULL
};

const struct decoder_plugin flac_decoder_plugin = {
	.name = "flac",
	.stream_decode = flac_decode,
#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT > 7
	.file_decode = flac_filedecode,
#endif /* FLAC_API_VERSION_CURRENT >= 7 */
	.tag_dup = flac_tag_dup,
	.suffixes = flac_suffixes,
	.mime_types = flac_mime_types,
#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT > 7
	.container_scan = flac_cue_track,
#endif /* FLAC_API_VERSION_CURRENT >= 7 */
};
