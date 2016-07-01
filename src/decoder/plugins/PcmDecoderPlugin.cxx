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

#include "config.h"
#include "PcmDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "CheckAudioFormat.hxx"
#include "input/InputStream.hxx"
#include "system/ByteOrder.hxx"
#include "util/Error.hxx"
#include "util/ByteReverse.hxx"
#include "util/NumberParser.hxx"
#include "util/MimeType.hxx"
#include "Log.hxx"

#include <string.h>

static void
pcm_stream_decode(Decoder &decoder, InputStream &is)
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
		Error error;

		/* MIME type parameters according to RFC 2586 */
		auto i = mime_parameters.find("rate");
		if (i != mime_parameters.end()) {
			const char *s = i->second.c_str();
			char *endptr;
			unsigned value = ParseUnsigned(s, &endptr);
			if (endptr == s || *endptr != 0) {
				FormatWarning(audio_format_domain,
					      "Failed to parse sample rate: %s",
					      s);
				return;
			}

			if (!audio_check_sample_rate(value, error)) {
				LogError(error);
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
				FormatWarning(audio_format_domain,
					      "Failed to parse sample rate: %s",
					      s);
				return;
			}

			if (!audio_check_channel_count(value, error)) {
				LogError(error);
				return;
			}

			audio_format.channels = value;
		}
	}

	if (audio_format.sample_rate == 0) {
		FormatWarning(audio_format_domain,
			      "Missing 'rate' parameter: %s",
			      mime);
		return;
	}

	const auto frame_size = audio_format.GetFrameSize();

	const auto total_time = is.KnownSize()
		? SignedSongTime::FromScale<uint64_t>(is.GetSize() / frame_size,
						      audio_format.sample_rate)
		: SignedSongTime::Negative();

	decoder_initialized(decoder, audio_format,
			    is.IsSeekable(), total_time);

	DecoderCommand cmd;
	do {
		char buffer[4096];

		size_t nbytes = decoder_read(decoder, is,
					     buffer, sizeof(buffer));

		if (nbytes == 0 && is.LockIsEOF())
			break;

		if (reverse_endian)
			/* make sure we deliver samples in host byte order */
			reverse_bytes_16((uint16_t *)buffer,
					 (uint16_t *)buffer,
					 (uint16_t *)(buffer + nbytes));

		cmd = nbytes > 0
			? decoder_data(decoder, is,
				       buffer, nbytes, 0)
			: decoder_get_command(decoder);
		if (cmd == DecoderCommand::SEEK) {
			uint64_t frame = decoder_seek_where_frame(decoder);
			offset_type offset = frame * frame_size;

			Error error;
			if (is.LockSeek(offset, error)) {
				decoder_command_finished(decoder);
			} else {
				LogError(error);
				decoder_seek_error(decoder);
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
