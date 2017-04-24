/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "PcmDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "CheckAudioFormat.hxx"
#include "input/InputStream.hxx"
#include "system/ByteOrder.hxx"
#include "util/Domain.hxx"
#include "util/ByteReverse.hxx"
#include "util/StaticFifoBuffer.hxx"
#include "util/NumberParser.hxx"
#include "util/MimeType.hxx"
#include "Log.hxx"

#include <stdexcept>

#include <assert.h>
#include <string.h>

static constexpr Domain pcm_decoder_domain("pcm_decoder");

template<typename B>
static bool
FillBuffer(DecoderClient &client, InputStream &is, B &buffer)
{
	buffer.Shift();
	auto w = buffer.Write();
	if (w.IsEmpty())
		return true;

	size_t nbytes = decoder_read(client, is, w.data, w.size);
	if (nbytes == 0 && is.LockIsEOF())
		return false;

	buffer.Append(nbytes);
	return true;
}

static void
pcm_stream_decode(DecoderClient &client, InputStream &is)
{
	AudioFormat audio_format = {
		44100,
		SampleFormat::S16,
		2,
	};

	const char *const mime = is.GetMimeType();

	const bool l16 = mime != nullptr &&
		GetMimeTypeBase(mime) == "audio/L16";
	const bool is_float = mime != nullptr &&
		GetMimeTypeBase(mime) == "audio/x-mpd-float";
	if (l16 || is_float) {
		audio_format.sample_rate = 0;
		audio_format.channels = 1;
	}

	const bool reverse_endian = (l16 && IsLittleEndian()) ||
		(mime != nullptr &&
		 strcmp(mime, "audio/x-mpd-cdda-pcm-reverse") == 0);

	if (is_float)
		audio_format.format = SampleFormat::FLOAT;

	{
		const auto mime_parameters = ParseMimeTypeParameters(mime);

		/* MIME type parameters according to RFC 2586 */
		auto i = mime_parameters.find("rate");
		if (i != mime_parameters.end()) {
			const char *s = i->second.c_str();
			char *endptr;
			unsigned value = ParseUnsigned(s, &endptr);
			if (endptr == s || *endptr != 0) {
				FormatWarning(pcm_decoder_domain,
					      "Failed to parse sample rate: %s",
					      s);
				return;
			}

			try {
				CheckSampleRate(value);
			} catch (const std::runtime_error &e) {
				LogError(e);
				return;
			}

			audio_format.sample_rate = value;
		}

		i = mime_parameters.find("channels");
		if (i != mime_parameters.end()) {
			const char *s = i->second.c_str();
			char *endptr;
			unsigned value = ParseUnsigned(s, &endptr);
			if (endptr == s || *endptr != 0) {
				FormatWarning(pcm_decoder_domain,
					      "Failed to parse sample rate: %s",
					      s);
				return;
			}

			try {
				CheckChannelCount(value);
			} catch (const std::runtime_error &e) {
				LogError(e);
				return;
			}

			audio_format.channels = value;
		}
	}

	if (audio_format.sample_rate == 0) {
		FormatWarning(pcm_decoder_domain,
			      "Missing 'rate' parameter: %s",
			      mime);
		return;
	}

	const auto out_frame_size = audio_format.GetFrameSize();
	const auto in_frame_size = out_frame_size;

	const auto total_time = is.KnownSize()
		? SignedSongTime::FromScale<uint64_t>(is.GetSize() / in_frame_size,
						      audio_format.sample_rate)
		: SignedSongTime::Negative();

	client.Ready(audio_format, is.IsSeekable(), total_time);

	StaticFifoBuffer<uint8_t, 4096> buffer;

	DecoderCommand cmd;
	do {
		if (!FillBuffer(client, is, buffer))
			break;

		auto r = buffer.Read();
		/* round down to the nearest frame size, because we
		   must not pass partial frames to
		   DecoderClient::SubmitData() */
		r.size -= r.size % in_frame_size;
		buffer.Consume(r.size);

		if (reverse_endian)
			/* make sure we deliver samples in host byte order */
			reverse_bytes_16((uint16_t *)r.data,
					 (uint16_t *)r.data,
					 (uint16_t *)(r.data + r.size));

		cmd = !r.IsEmpty()
			? client.SubmitData(is, r.data, r.size, 0)
			: client.GetCommand();
		if (cmd == DecoderCommand::SEEK) {
			uint64_t frame = client.GetSeekFrame();
			offset_type offset = frame * in_frame_size;

			try {
				is.LockSeek(offset);
				buffer.Clear();
				client.CommandFinished();
			} catch (const std::runtime_error &e) {
				LogError(e);
				client.SeekError();
			}

			cmd = DecoderCommand::NONE;
		}
	} while (cmd == DecoderCommand::NONE);
}

static const char *const pcm_mime_types[] = {
	/* RFC 2586 */
	"audio/L16",

	/* MPD-specific: float32 native-endian */
	"audio/x-mpd-float",

	/* for streams obtained by the cdio_paranoia input plugin */
	"audio/x-mpd-cdda-pcm",

	/* same as above, but with reverse byte order */
	"audio/x-mpd-cdda-pcm-reverse",

	nullptr
};

const struct DecoderPlugin pcm_decoder_plugin = {
	"pcm",
	nullptr,
	nullptr,
	pcm_stream_decode,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	pcm_mime_types,
};
