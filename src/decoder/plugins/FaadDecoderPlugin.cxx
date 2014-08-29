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

#include "config.h"
#include "FaadDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "../DecoderBuffer.hxx"
#include "input/InputStream.hxx"
#include "CheckAudioFormat.hxx"
#include "tag/TagHandler.hxx"
#include "util/ConstBuffer.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <neaacdec.h>

#include <assert.h>
#include <string.h>
#include <unistd.h>

static const unsigned adts_sample_rates[] =
    { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
	16000, 12000, 11025, 8000, 7350, 0, 0, 0
};

static constexpr Domain faad_decoder_domain("faad_decoder");

/**
 * Check whether the buffer head is an AAC frame, and return the frame
 * length.  Returns 0 if it is not a frame.
 */
static size_t
adts_check_frame(const unsigned char *data)
{
	/* check syncword */
	if (!((data[0] == 0xFF) && ((data[1] & 0xF6) == 0xF0)))
		return 0;

	return (((unsigned int)data[3] & 0x3) << 11) |
		(((unsigned int)data[4]) << 3) |
		(data[5] >> 5);
}

/**
 * Find the next AAC frame in the buffer.  Returns 0 if no frame is
 * found or if not enough data is available.
 */
static size_t
adts_find_frame(DecoderBuffer *buffer)
{
	while (true) {
		auto data = ConstBuffer<uint8_t>::FromVoid(decoder_buffer_need(buffer, 8));
		if (data.IsNull())
			/* failed */
			return 0;

		/* find the 0xff marker */
		const uint8_t *p = (const uint8_t *)
			memchr(data.data, 0xff, data.size);
		if (p == nullptr) {
			/* no marker - discard the buffer */
			decoder_buffer_clear(buffer);
			continue;
		}

		if (p > data.data) {
			/* discard data before 0xff */
			decoder_buffer_consume(buffer, p - data.data);
			continue;
		}

		/* is it a frame? */
		const size_t frame_length = adts_check_frame(data.data);
		if (frame_length == 0) {
			/* it's just some random 0xff byte; discard it
			   and continue searching */
			decoder_buffer_consume(buffer, 1);
			continue;
		}

		if (decoder_buffer_need(buffer, frame_length).IsNull()) {
			/* not enough data; discard this frame to
			   prevent a possible buffer overflow */
			decoder_buffer_clear(buffer);
			continue;
		}

		/* found a full frame! */
		return frame_length;
	}
}

static float
adts_song_duration(DecoderBuffer *buffer)
{
	const InputStream &is = decoder_buffer_get_stream(buffer);
	const bool estimate = !is.CheapSeeking();
	if (estimate && !is.KnownSize())
		return -1;

	unsigned sample_rate = 0;

	/* Read all frames to ensure correct time and bitrate */
	unsigned frames = 0;
	for (;; frames++) {
		const unsigned frame_length = adts_find_frame(buffer);
		if (frame_length == 0)
			break;

		if (frames == 0) {
			auto data = ConstBuffer<uint8_t>::FromVoid(decoder_buffer_read(buffer));
			assert(!data.IsEmpty());
			assert(frame_length <= data.size);

			sample_rate = adts_sample_rates[(data.data[2] & 0x3c) >> 2];
			if (sample_rate == 0)
				break;
		}

		decoder_buffer_consume(buffer, frame_length);

		if (estimate && frames == 128) {
			/* if this is a remote file, don't slurp the
			   whole file just for checking the song
			   duration; instead, stop after some time and
			   extrapolate the song duration from what we
			   have until now */

			const auto offset = is.GetOffset()
				- decoder_buffer_available(buffer);
			if (offset <= 0)
				return -1;

			const auto file_size = is.GetSize();
			frames = (frames * file_size) / offset;
			break;
		}
	}

	if (sample_rate == 0)
		return -1;

	float frames_per_second = (float)sample_rate / 1024.0;
	assert(frames_per_second > 0);

	return (float)frames / frames_per_second;
}

static float
faad_song_duration(DecoderBuffer *buffer, InputStream &is)
{
	auto data = ConstBuffer<uint8_t>::FromVoid(decoder_buffer_need(buffer, 5));
	if (data.IsNull())
		return -1;

	size_t tagsize = 0;
	if (data.size >= 10 && !memcmp(data.data, "ID3", 3)) {
		/* skip the ID3 tag */

		tagsize = (data.data[6] << 21) | (data.data[7] << 14) |
		    (data.data[8] << 7) | (data.data[9] << 0);

		tagsize += 10;

		if (!decoder_buffer_skip(buffer, tagsize))
			return -1;

		data = ConstBuffer<uint8_t>::FromVoid(decoder_buffer_need(buffer, 5));
		if (data.IsNull())
			return -1;
	}

	if (data.size >= 8 && adts_check_frame(data.data) > 0) {
		/* obtain the duration from the ADTS header */

		if (!is.IsSeekable())
			return -1;

		float song_length = adts_song_duration(buffer);

		is.LockSeek(tagsize, IgnoreError());

		decoder_buffer_clear(buffer);

		return song_length;
	} else if (data.size >= 5 && memcmp(data.data, "ADIF", 4) == 0) {
		/* obtain the duration from the ADIF header */

		if (!is.KnownSize())
			return -1;

		size_t skip_size = (data.data[4] & 0x80) ? 9 : 0;

		if (8 + skip_size > data.size)
			/* not enough data yet; skip parsing this
			   header */
			return -1;

		unsigned bit_rate = ((data.data[4 + skip_size] & 0x0F) << 19) |
			(data.data[5 + skip_size] << 11) |
			(data.data[6 + skip_size] << 3) |
			(data.data[7 + skip_size] & 0xE0);

		const auto size = is.GetSize();
		if (bit_rate == 0)
			return -1;

		return size * 8.0 / bit_rate;
	} else
		return -1;
}

static NeAACDecHandle
faad_decoder_new()
{
	const NeAACDecHandle decoder = NeAACDecOpen();

	NeAACDecConfigurationPtr config =
		NeAACDecGetCurrentConfiguration(decoder);
	config->outputFormat = FAAD_FMT_16BIT;
	config->downMatrix = 1;
	config->dontUpSampleImplicitSBR = 0;
	NeAACDecSetConfiguration(decoder, config);

	return decoder;
}

/**
 * Wrapper for NeAACDecInit() which works around some API
 * inconsistencies in libfaad.
 */
static bool
faad_decoder_init(NeAACDecHandle decoder, DecoderBuffer *buffer,
		  AudioFormat &audio_format, Error &error)
{
	auto data = ConstBuffer<uint8_t>::FromVoid(decoder_buffer_read(buffer));
	if (data.IsEmpty()) {
		error.Set(faad_decoder_domain, "Empty file");
		return false;
	}

	uint8_t channels;
	uint32_t sample_rate;
#ifdef HAVE_FAAD_LONG
	/* neaacdec.h declares all arguments as "unsigned long", but
	   internally expects uint32_t pointers.  To avoid gcc
	   warnings, use this workaround. */
	unsigned long *sample_rate_p = (unsigned long *)(void *)&sample_rate;
#else
	uint32_t *sample_rate_p = &sample_rate;
#endif
	long nbytes = NeAACDecInit(decoder,
				   /* deconst hack, libfaad requires this */
				   const_cast<unsigned char *>(data.data),
				   data.size,
				   sample_rate_p, &channels);
	if (nbytes < 0) {
		error.Set(faad_decoder_domain, "Not an AAC stream");
		return false;
	}

	decoder_buffer_consume(buffer, nbytes);

	return audio_format_init_checked(audio_format, sample_rate,
					 SampleFormat::S16, channels, error);
}

/**
 * Wrapper for NeAACDecDecode() which works around some API
 * inconsistencies in libfaad.
 */
static const void *
faad_decoder_decode(NeAACDecHandle decoder, DecoderBuffer *buffer,
		    NeAACDecFrameInfo *frame_info)
{
	auto data = ConstBuffer<uint8_t>::FromVoid(decoder_buffer_read(buffer));
	if (data.IsEmpty())
		return nullptr;

	return NeAACDecDecode(decoder, frame_info,
			      /* deconst hack, libfaad requires this */
			      const_cast<uint8_t *>(data.data),
			      data.size);
}

/**
 * Get a song file's total playing time in seconds, as a float.
 * Returns 0 if the duration is unknown, and a negative value if the
 * file is invalid.
 */
static float
faad_get_file_time_float(InputStream &is)
{
	DecoderBuffer *buffer =
		decoder_buffer_new(nullptr, is,
				   FAAD_MIN_STREAMSIZE * MAX_CHANNELS);
	float length = faad_song_duration(buffer, is);

	if (length < 0) {
		NeAACDecHandle decoder = faad_decoder_new();

		decoder_buffer_fill(buffer);

		AudioFormat audio_format;
		if (faad_decoder_init(decoder, buffer, audio_format,
				      IgnoreError()))
			length = 0;

		NeAACDecClose(decoder);
	}

	decoder_buffer_free(buffer);

	return length;
}

/**
 * Get a song file's total playing time in seconds, as an int.
 * Returns 0 if the duration is unknown, and a negative value if the
 * file is invalid.
 */
static int
faad_get_file_time(InputStream &is)
{
	float length = faad_get_file_time_float(is);
	if (length < 0)
		return -1;

	return int(length + 0.5);
}

static void
faad_stream_decode(Decoder &mpd_decoder, InputStream &is,
		   DecoderBuffer *buffer, const NeAACDecHandle decoder)
{
	const float total_time = faad_song_duration(buffer, is);

	if (adts_find_frame(buffer) == 0)
		return;

	/* initialize it */

	Error error;
	AudioFormat audio_format;
	if (!faad_decoder_init(decoder, buffer, audio_format, error)) {
		LogError(error);
		return;
	}

	/* initialize the MPD core */

	decoder_initialized(mpd_decoder, audio_format, false, total_time);

	/* the decoder loop */

	DecoderCommand cmd;
	unsigned bit_rate = 0;
	do {
		/* find the next frame */

		const size_t frame_size = adts_find_frame(buffer);
		if (frame_size == 0)
			/* end of file */
			break;

		/* decode it */

		NeAACDecFrameInfo frame_info;
		const void *const decoded =
			faad_decoder_decode(decoder, buffer, &frame_info);

		if (frame_info.error > 0) {
			FormatWarning(faad_decoder_domain,
				      "error decoding AAC stream: %s",
				      NeAACDecGetErrorMessage(frame_info.error));
			break;
		}

		if (frame_info.channels != audio_format.channels) {
			FormatDefault(faad_decoder_domain,
				      "channel count changed from %u to %u",
				      audio_format.channels, frame_info.channels);
			break;
		}

		if (frame_info.samplerate != audio_format.sample_rate) {
			FormatDefault(faad_decoder_domain,
				      "sample rate changed from %u to %lu",
				      audio_format.sample_rate,
				      (unsigned long)frame_info.samplerate);
			break;
		}

		decoder_buffer_consume(buffer, frame_info.bytesconsumed);

		/* update bit rate and position */

		if (frame_info.samples > 0) {
			bit_rate = frame_info.bytesconsumed * 8.0 *
			    frame_info.channels * audio_format.sample_rate /
			    frame_info.samples / 1000 + 0.5;
		}

		/* send PCM samples to MPD */

		cmd = decoder_data(mpd_decoder, is, decoded,
				   (size_t)frame_info.samples * 2,
				   bit_rate);
	} while (cmd != DecoderCommand::STOP);
}

static void
faad_stream_decode(Decoder &mpd_decoder, InputStream &is)
{
	DecoderBuffer *buffer =
		decoder_buffer_new(&mpd_decoder, is,
				   FAAD_MIN_STREAMSIZE * MAX_CHANNELS);

	/* create the libfaad decoder */

	const NeAACDecHandle decoder = faad_decoder_new();

	faad_stream_decode(mpd_decoder, is, buffer, decoder);

	/* cleanup */

	NeAACDecClose(decoder);
	decoder_buffer_free(buffer);
}

static bool
faad_scan_stream(InputStream &is,
		 const struct tag_handler *handler, void *handler_ctx)
{
	int file_time = faad_get_file_time(is);
	if (file_time < 0)
		return false;

	tag_handler_invoke_duration(handler, handler_ctx, file_time);
	return true;
}

static const char *const faad_suffixes[] = { "aac", nullptr };
static const char *const faad_mime_types[] = {
	"audio/aac", "audio/aacp", nullptr
};

const DecoderPlugin faad_decoder_plugin = {
	"faad",
	nullptr,
	nullptr,
	faad_stream_decode,
	nullptr,
	nullptr,
	faad_scan_stream,
	nullptr,
	faad_suffixes,
	faad_mime_types,
};
