/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "FlacDomain.hxx"
#include "CheckAudioFormat.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

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
FlacDecoder::Initialize(unsigned sample_rate, unsigned bits_per_sample,
			unsigned channels, FLAC__uint64 total_frames)
{
	assert(!initialized);
	assert(!unsupported);

	auto sample_format = flac_sample_format(bits_per_sample);
	if (sample_format == SampleFormat::UNDEFINED) {
		FormatWarning(flac_domain, "Unsupported FLAC bit depth: %u",
			      bits_per_sample);
		unsupported = true;
		return false;
	}

	::Error error;
	if (!audio_format_init_checked(audio_format,
				       sample_rate,
				       sample_format,
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

	decoder_initialized(*GetDecoder(), audio_format,
			    GetInputStream().IsSeekable(),
			    duration);

	initialized = true;
	return true;
}

inline void
FlacDecoder::OnStreamInfo(const FLAC__StreamMetadata_StreamInfo &stream_info)
{
	if (initialized)
		return;

	Initialize(stream_info.sample_rate,
		   stream_info.bits_per_sample,
		   stream_info.channels,
		   stream_info.total_samples);
}

inline void
FlacDecoder::OnVorbisComment(const FLAC__StreamMetadata_VorbisComment &vc)
{
	ReplayGainInfo rgi;
	if (flac_parse_replay_gain(rgi, vc))
		decoder_replay_gain(*GetDecoder(), &rgi);

	decoder_mixramp(*GetDecoder(), flac_parse_mixramp(vc));

	tag = flac_vorbis_comments_to_tag(&vc);
}

void
FlacDecoder::OnMetadata(const FLAC__StreamMetadata &metadata)
{
	if (unsupported)
		return;

	switch (metadata.type) {
	case FLAC__METADATA_TYPE_STREAMINFO:
		OnStreamInfo(metadata.data.stream_info);
		break;

	case FLAC__METADATA_TYPE_VORBIS_COMMENT:
		OnVorbisComment(metadata.data.vorbis_comment);
		break;

	default:
		break;
	}
}

inline bool
FlacDecoder::OnFirstFrame(const FLAC__FrameHeader &header)
{
	if (unsupported)
		return false;

	return Initialize(header.sample_rate,
			  header.bits_per_sample,
			  header.channels,
			  /* unknown duration */
			  0);
}

FLAC__uint64
FlacDecoder::GetDeltaPosition(const FLAC__StreamDecoder &sd)
{
	FLAC__uint64 nbytes;
	if (!FLAC__stream_decoder_get_decode_position(&sd, &nbytes))
		return 0;

	if (position > 0 && nbytes > position) {
		nbytes -= position;
		position += nbytes;
	} else {
		position = nbytes;
		nbytes = 0;
	}

	return nbytes;
}

FLAC__StreamDecoderWriteStatus
FlacDecoder::OnWrite(const FLAC__Frame &frame,
		     const FLAC__int32 *const buf[],
		     FLAC__uint64 nbytes)
{
	if (!initialized && !OnFirstFrame(frame.header))
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	size_t buffer_size = frame.header.blocksize * frame_size;
	void *data = buffer.Get(buffer_size);

	flac_convert(data, frame.header.channels,
		     audio_format.format, buf,
		     frame.header.blocksize);

	unsigned bit_rate = nbytes * 8 * frame.header.sample_rate /
		(1000 * frame.header.blocksize);

	auto cmd = decoder_data(*GetDecoder(), GetInputStream(),
				data, buffer_size,
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
