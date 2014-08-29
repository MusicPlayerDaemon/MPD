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
#include "PcmDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "util/Error.hxx"
#include "util/ByteReverse.hxx"
#include "Log.hxx"

#include <string.h>

static void
pcm_stream_decode(Decoder &decoder, InputStream &is)
{
	static constexpr AudioFormat audio_format = {
		44100,
		SampleFormat::S16,
		2,
	};

	const char *const mime = is.GetMimeType();
	const bool reverse_endian = mime != nullptr &&
		strcmp(mime, "audio/x-mpd-cdda-pcm-reverse") == 0;

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
