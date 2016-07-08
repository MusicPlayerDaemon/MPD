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
	 position(0),
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

bool
flac_data::Initialize(unsigned sample_rate, unsigned bits_per_sample,
		      unsigned channels, FLAC__uint64 total_frames)
{
	assert(!initialized);
	assert(!unsupported);

	::Error error;
	if (!audio_format_init_checked(audio_format,
				       sample_rate,
				       flac_sample_format(bits_per_sample),
				       channels, error)) {
		LogError(error);
		unsupported = true;
		return false;
	}

	frame_size = audio_format.GetFrameSize();

	const auto duration = total_frames > 0
		? SignedSongTime::FromScale<uint64_t>(total_frames,
						      audio_format.sample_rate)
		: SignedSongTime::Negative();

	decoder_initialized(decoder, audio_format,
			    input_stream.IsSeekable(),
			    duration);

	initialized = true;
	return true;
}

static void
flac_got_stream_info(struct flac_data *data,
		     const FLAC__StreamMetadata_StreamInfo *stream_info)
{
	if (data->initialized || data->unsupported)
		return;

	data->Initialize(stream_info->sample_rate,
			 stream_info->bits_per_sample,
			 stream_info->channels,
			 stream_info->total_samples);
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

	return data->Initialize(header->sample_rate,
				header->bits_per_sample,
				header->channels,
				/* unknown duration */
				0);
}

FLAC__StreamDecoderWriteStatus
flac_common_write(struct flac_data *data, const FLAC__Frame * frame,
		  const FLAC__int32 *const buf[],
		  FLAC__uint64 nbytes)
{
	void *buffer;

	if (!data->initialized && !flac_got_first_frame(data, &frame->header))
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	size_t buffer_size = frame->header.blocksize * data->frame_size;
	buffer = data->buffer.Get(buffer_size);

	flac_convert(buffer, frame->header.channels,
		     data->audio_format.format, buf,
		     0, frame->header.blocksize);

	unsigned bit_rate = nbytes * 8 * frame->header.sample_rate /
		(1000 * frame->header.blocksize);

	auto cmd = decoder_data(data->decoder, data->input_stream,
				buffer, buffer_size,
				bit_rate);
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
