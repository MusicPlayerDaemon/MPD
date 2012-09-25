/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
 * Common data structures and functions used by FLAC and OggFLAC
 */

#include "config.h"
#include "_flac_common.h"
#include "flac_metadata.h"
#include "flac_pcm.h"
#include "audio_check.h"

#include <glib.h>

#include <assert.h>

void
flac_data_init(struct flac_data *data, struct decoder * decoder,
	       struct input_stream *input_stream)
{
	pcm_buffer_init(&data->buffer);

	data->unsupported = false;
	data->initialized = false;
	data->total_frames = 0;
	data->first_frame = 0;
	data->next_frame = 0;

	data->position = 0;
	data->decoder = decoder;
	data->input_stream = input_stream;
	data->tag = NULL;
}

void
flac_data_deinit(struct flac_data *data)
{
	pcm_buffer_deinit(&data->buffer);

	if (data->tag != NULL)
		tag_free(data->tag);
}

static enum sample_format
flac_sample_format(unsigned bits_per_sample)
{
	switch (bits_per_sample) {
	case 8:
		return SAMPLE_FORMAT_S8;

	case 16:
		return SAMPLE_FORMAT_S16;

	case 24:
		return SAMPLE_FORMAT_S24_P32;

	case 32:
		return SAMPLE_FORMAT_S32;

	default:
		return SAMPLE_FORMAT_UNDEFINED;
	}
}

static void
flac_got_stream_info(struct flac_data *data,
		     const FLAC__StreamMetadata_StreamInfo *stream_info)
{
	if (data->initialized || data->unsupported)
		return;

	GError *error = NULL;
	if (!audio_format_init_checked(&data->audio_format,
				       stream_info->sample_rate,
				       flac_sample_format(stream_info->bits_per_sample),
				       stream_info->channels, &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		data->unsupported = true;
		return;
	}

	data->frame_size = audio_format_frame_size(&data->audio_format);

	if (data->total_frames == 0)
		data->total_frames = stream_info->total_samples;

	data->initialized = true;
}

void flac_metadata_common_cb(const FLAC__StreamMetadata * block,
			     struct flac_data *data)
{
	if (data->unsupported)
		return;

	struct replay_gain_info rgi;
	char *mixramp_start;
	char *mixramp_end;
	float replay_gain_db = 0;

	switch (block->type) {
	case FLAC__METADATA_TYPE_STREAMINFO:
		flac_got_stream_info(data, &block->data.stream_info);
		break;

	case FLAC__METADATA_TYPE_VORBIS_COMMENT:
		if (flac_parse_replay_gain(&rgi, block))
			replay_gain_db = decoder_replay_gain(data->decoder, &rgi);

		if (flac_parse_mixramp(&mixramp_start, &mixramp_end, block))
			decoder_mixramp(data->decoder, replay_gain_db,
					mixramp_start, mixramp_end);

		if (data->tag != NULL)
			flac_vorbis_comments_to_tag(data->tag, NULL,
						    &block->data.vorbis_comment);

	default:
		break;
	}
}

void flac_error_common_cb(const FLAC__StreamDecoderErrorStatus status,
			  struct flac_data *data)
{
	if (decoder_get_command(data->decoder) == DECODE_COMMAND_STOP)
		return;

	g_warning("%s", FLAC__StreamDecoderErrorStatusString[status]);
}

/**
 * This function attempts to call decoder_initialized() in case there
 * was no STREAMINFO block.  This is allowed for nonseekable streams,
 * where the server sends us only a part of the file, without
 * providing the STREAMINFO block from the beginning of the file
 * (e.g. when seeking with SqueezeBox Server).
 */
static bool
flac_got_first_frame(struct flac_data *data, const FLAC__FrameHeader *header)
{
	if (data->unsupported)
		return false;

	GError *error = NULL;
	if (!audio_format_init_checked(&data->audio_format,
				       header->sample_rate,
				       flac_sample_format(header->bits_per_sample),
				       header->channels, &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		data->unsupported = true;
		return false;
	}

	data->frame_size = audio_format_frame_size(&data->audio_format);

	decoder_initialized(data->decoder, &data->audio_format,
			    data->input_stream->seekable,
			    (float)data->total_frames /
			    (float)data->audio_format.sample_rate);

	data->initialized = true;

	return true;
}

FLAC__StreamDecoderWriteStatus
flac_common_write(struct flac_data *data, const FLAC__Frame * frame,
		  const FLAC__int32 *const buf[],
		  FLAC__uint64 nbytes)
{
	enum decoder_command cmd;
	void *buffer;
	unsigned bit_rate;

	if (!data->initialized && !flac_got_first_frame(data, &frame->header))
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	size_t buffer_size = frame->header.blocksize * data->frame_size;
	buffer = pcm_buffer_get(&data->buffer, buffer_size);

	flac_convert(buffer, frame->header.channels,
		     data->audio_format.format, buf,
		     0, frame->header.blocksize);

	if (nbytes > 0)
		bit_rate = nbytes * 8 * frame->header.sample_rate /
			(1000 * frame->header.blocksize);
	else
		bit_rate = 0;

	cmd = decoder_data(data->decoder, data->input_stream,
			   buffer, buffer_size,
			   bit_rate);
	data->next_frame += frame->header.blocksize;
	switch (cmd) {
	case DECODE_COMMAND_NONE:
	case DECODE_COMMAND_START:
		break;

	case DECODE_COMMAND_STOP:
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	case DECODE_COMMAND_SEEK:
		return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}
