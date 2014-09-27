/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "FlacCommon.hxx"
#include "FlacMetadata.hxx"
#include "FlacPcm.hxx"
#include "CheckAudioFormat.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

flac_data::flac_data(Decoder &_decoder,
		     InputStream &_input_stream)
	:FlacInput(_input_stream, &_decoder),
	 initialized(false), unsupported(false),
	 total_frames(0), first_frame(0), next_frame(0), position(0),
	 decoder(_decoder), input_stream(_input_stream)
{
}

static SampleFormat
flac_sample_format(unsigned bits_per_sample)
{
	switch (bits_per_sample) {
	case 8:
		return SampleFormat::S8;

	case 16:
		return SampleFormat::S16;

	case 24:
		return SampleFormat::S24_P32;

	case 32:
		return SampleFormat::S32;

	default:
		return SampleFormat::UNDEFINED;
	}
}

static void
flac_got_stream_info(struct flac_data *data,
		     const FLAC__StreamMetadata_StreamInfo *stream_info)
{
	if (data->initialized || data->unsupported)
		return;

	Error error;
	if (!audio_format_init_checked(data->audio_format,
				       stream_info->sample_rate,
				       flac_sample_format(stream_info->bits_per_sample),
				       stream_info->channels, error)) {
		LogError(error);
		data->unsupported = true;
		return;
	}

	data->frame_size = data->audio_format.GetFrameSize();

	if (data->total_frames == 0)
		data->total_frames = stream_info->total_samples;

	data->initialized = true;
}

void flac_metadata_common_cb(const FLAC__StreamMetadata * block,
			     struct flac_data *data)
{
	if (data->unsupported)
		return;

	ReplayGainInfo rgi;

	switch (block->type) {
	case FLAC__METADATA_TYPE_STREAMINFO:
		flac_got_stream_info(data, &block->data.stream_info);
		break;

	case FLAC__METADATA_TYPE_VORBIS_COMMENT:
		if (flac_parse_replay_gain(rgi, block->data.vorbis_comment))
			decoder_replay_gain(data->decoder, &rgi);

		decoder_mixramp(data->decoder,
				flac_parse_mixramp(block->data.vorbis_comment));

		data->tag = flac_vorbis_comments_to_tag(&block->data.vorbis_comment);
		break;

	default:
		break;
	}
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

	Error error;
	if (!audio_format_init_checked(data->audio_format,
				       header->sample_rate,
				       flac_sample_format(header->bits_per_sample),
				       header->channels, error)) {
		LogError(error);
		data->unsupported = true;
		return false;
	}

	data->frame_size = data->audio_format.GetFrameSize();

	const auto duration = SongTime::FromScale<uint64_t>(data->total_frames,
							    data->audio_format.sample_rate);

	decoder_initialized(data->decoder, data->audio_format,
			    data->input_stream.IsSeekable(),
			    duration);

	data->initialized = true;

	return true;
}

FLAC__StreamDecoderWriteStatus
flac_common_write(struct flac_data *data, const FLAC__Frame * frame,
		  const FLAC__int32 *const buf[],
		  FLAC__uint64 nbytes)
{
	void *buffer;
	unsigned bit_rate;

	if (!data->initialized && !flac_got_first_frame(data, &frame->header))
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	size_t buffer_size = frame->header.blocksize * data->frame_size;
	buffer = data->buffer.Get(buffer_size);

	flac_convert(buffer, frame->header.channels,
		     data->audio_format.format, buf,
		     0, frame->header.blocksize);

	if (nbytes > 0)
		bit_rate = nbytes * 8 * frame->header.sample_rate /
			(1000 * frame->header.blocksize);
	else
		bit_rate = 0;

	auto cmd = decoder_data(data->decoder, data->input_stream,
				buffer, buffer_size,
				bit_rate);
	data->next_frame += frame->header.blocksize;
	switch (cmd) {
	case DecoderCommand::NONE:
	case DecoderCommand::START:
		break;

	case DecoderCommand::STOP:
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	case DecoderCommand::SEEK:
		return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}
