/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "MpcdecDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "pcm/CheckAudioFormat.hxx"
#include "pcm/Traits.hxx"
#include "tag/Handler.hxx"
#include "util/Domain.hxx"
#include "util/Clamp.hxx"
#include "util/ScopeExit.hxx"
#include "Log.hxx"

#include <mpc/mpcdec.h>

#include <cmath>
#include <iterator>

struct mpc_decoder_data {
	InputStream &is;
	DecoderClient *client;

	mpc_decoder_data(InputStream &_is, DecoderClient *_client)
		:is(_is), client(_client) {}
};

static constexpr Domain mpcdec_domain("mpcdec");

static constexpr SampleFormat mpcdec_sample_format = SampleFormat::S24_P32;
using MpcdecSampleTraits = SampleTraits<mpcdec_sample_format>;

static mpc_int32_t
mpc_read_cb(mpc_reader *reader, void *ptr, mpc_int32_t size)
{
	auto *data =
		(struct mpc_decoder_data *)reader->data;

	return decoder_read(data->client, data->is, ptr, size);
}

static mpc_bool_t
mpc_seek_cb(mpc_reader *reader, mpc_int32_t offset)
{
	auto *data =
		(struct mpc_decoder_data *)reader->data;

	try {
		data->is.LockSeek(offset);
		return true;
	} catch (...) {
		return false;
	}
}

static mpc_int32_t
mpc_tell_cb(mpc_reader *reader)
{
	auto *data =
		(struct mpc_decoder_data *)reader->data;

	return (long)data->is.GetOffset();
}

static mpc_bool_t
mpc_canseek_cb(mpc_reader *reader)
{
	auto *data =
		(struct mpc_decoder_data *)reader->data;

	return data->is.IsSeekable();
}

static mpc_int32_t
mpc_getsize_cb(mpc_reader *reader)
{
	auto *data =
		(struct mpc_decoder_data *)reader->data;

	if (!data->is.KnownSize())
		return -1;

	return data->is.GetSize();
}

/* this _looks_ performance-critical, don't de-inline -- eric */
static inline MpcdecSampleTraits::value_type
mpc_to_mpd_sample(MPC_SAMPLE_FORMAT sample)
{
	/* only doing 16-bit audio for now */
	MpcdecSampleTraits::value_type val;

	constexpr int bits = MpcdecSampleTraits::BITS;
	constexpr auto clip_min = MpcdecSampleTraits::MIN;
	constexpr auto clip_max = MpcdecSampleTraits::MAX;

#ifdef MPC_FIXED_POINT
	const int shift = bits - MPC_FIXED_POINT_SCALE_SHIFT;

	if (shift < 0)
		val = sample >> -shift;
	else
		val = sample << shift;
#else
	const int float_scale = 1 << (bits - 1);

	val = sample * float_scale;
#endif

	return Clamp(val, clip_min, clip_max);
}

static void
mpc_to_mpd_buffer(MpcdecSampleTraits::pointer dest,
		  const MPC_SAMPLE_FORMAT *src,
		  unsigned num_samples)
{
	while (num_samples-- > 0)
		*dest++ = mpc_to_mpd_sample(*src++);
}

static constexpr ReplayGainTuple
ImportMpcdecReplayGain(mpc_uint16_t gain, mpc_uint16_t peak) noexcept
{
	auto t = ReplayGainTuple::Undefined();

	if (gain != 0 && peak != 0) {
		t.gain = MPC_OLD_GAIN_REF - (gain  / 256.);
		t.peak = std::pow(10, peak / 256. / 20) / 32767;
	}

	return t;
}

static constexpr ReplayGainInfo
ImportMpcdecReplayGain(const mpc_streaminfo &info) noexcept
{
	auto rgi = ReplayGainInfo::Undefined();
	rgi.album = ImportMpcdecReplayGain(info.gain_album, info.peak_album);
	rgi.track = ImportMpcdecReplayGain(info.gain_title, info.peak_title);
	return rgi;
}

static void
mpcdec_decode(DecoderClient &client, InputStream &is)
{
	mpc_decoder_data data(is, &client);

	mpc_reader reader;
	reader.read = mpc_read_cb;
	reader.seek = mpc_seek_cb;
	reader.tell = mpc_tell_cb;
	reader.get_size = mpc_getsize_cb;
	reader.canseek = mpc_canseek_cb;
	reader.data = &data;

	mpc_demux *demux = mpc_demux_init(&reader);
	if (demux == nullptr) {
		if (client.GetCommand() != DecoderCommand::STOP)
			LogWarning(mpcdec_domain,
				   "Not a valid musepack stream");
		return;
	}

	AtScopeExit(demux) { mpc_demux_exit(demux); };

	mpc_streaminfo info;
	mpc_demux_get_info(demux, &info);

	auto audio_format = CheckAudioFormat(info.sample_freq,
					     mpcdec_sample_format,
					     info.channels);

	{
		const auto rgi = ImportMpcdecReplayGain(info);
		if (rgi.IsDefined())
			client.SubmitReplayGain(&rgi);
	}

	client.Ready(audio_format, is.IsSeekable(),
		     SongTime::FromS(mpc_streaminfo_get_length(&info)));

	DecoderCommand cmd = DecoderCommand::NONE;
	do {
		if (cmd == DecoderCommand::SEEK) {
			mpc_int64_t where = client.GetSeekFrame();
			bool success;

			success = mpc_demux_seek_sample(demux, where)
				== MPC_STATUS_OK;
			if (success)
				client.CommandFinished();
			else
				client.SeekError();
		}

		MPC_SAMPLE_FORMAT sample_buffer[MPC_DECODER_BUFFER_LENGTH];
		mpc_frame_info frame;
		frame.buffer = (MPC_SAMPLE_FORMAT *)sample_buffer;
		mpc_status status = mpc_demux_decode(demux, &frame);
		if (status != MPC_STATUS_OK) {
			LogWarning(mpcdec_domain,
				   "Failed to decode sample");
			break;
		}

		if (frame.bits == -1)
			break;

		if (frame.samples <= 0) {
			/* empty frame - this has been observed to
			   happen spuriously after seeking; skip this
			   obscure frame, and hope libmpcdec
			   recovers */
			cmd = client.GetCommand();
			continue;
		}

		mpc_uint32_t ret = frame.samples;
		ret *= info.channels;

		MpcdecSampleTraits::value_type chunk[std::size(sample_buffer)];
		mpc_to_mpd_buffer(chunk, sample_buffer, ret);

		long bit_rate = unsigned(frame.bits) * audio_format.sample_rate
			/ (1000 * frame.samples);

		cmd = client.SubmitData(is,
					chunk, ret * sizeof(chunk[0]),
					bit_rate);
	} while (cmd != DecoderCommand::STOP);
}

static SignedSongTime
mpcdec_get_file_duration(InputStream &is)
{
	mpc_decoder_data data(is, nullptr);

	mpc_reader reader;
	reader.read = mpc_read_cb;
	reader.seek = mpc_seek_cb;
	reader.tell = mpc_tell_cb;
	reader.get_size = mpc_getsize_cb;
	reader.canseek = mpc_canseek_cb;
	reader.data = &data;

	mpc_demux *demux = mpc_demux_init(&reader);
	if (demux == nullptr)
		return SignedSongTime::Negative();

	AtScopeExit(demux) { mpc_demux_exit(demux); };

	mpc_streaminfo info;
	mpc_demux_get_info(demux, &info);

	return SongTime::FromS(mpc_streaminfo_get_length(&info));
}

static bool
mpcdec_scan_stream(InputStream &is, TagHandler &handler)
{
	const auto duration = mpcdec_get_file_duration(is);
	if (duration.IsNegative())
		return false;

	handler.OnDuration(SongTime(duration));
	return true;
}

static const char *const mpcdec_suffixes[] = { "mpc", nullptr };

constexpr DecoderPlugin mpcdec_decoder_plugin =
	DecoderPlugin("mpcdec", mpcdec_decode, mpcdec_scan_stream)
	.WithSuffixes(mpcdec_suffixes);
