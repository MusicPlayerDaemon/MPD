/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "decoder/PcmDecoderPlugin.hxx"
#include "DecoderAPI.hxx"

extern "C" {
#include "util/byte_reverse.h"
}

#include <glib.h>
#include <unistd.h>
#include <stdio.h> /* for SEEK_SET */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "pcm"

static void
pcm_stream_decode(struct decoder *decoder, struct input_stream *is)
{
	static constexpr struct audio_format audio_format = {
		44100,
		SAMPLE_FORMAT_S16,
		2,
	};

	const char *const mime = input_stream_get_mime_type(is);
	const bool reverse_endian = mime != nullptr &&
		strcmp(mime, "audio/x-mpd-cdda-pcm-reverse") == 0;

	GError *error = nullptr;
	enum decoder_command cmd;

	double time_to_size = audio_format_time_to_size(&audio_format);

	float total_time = -1;
	const goffset size = input_stream_get_size(is);
	if (size >= 0)
		total_time = size / time_to_size;

	decoder_initialized(decoder, &audio_format,
			    input_stream_is_seekable(is), total_time);

	do {
		char buffer[4096];

		size_t nbytes = decoder_read(decoder, is,
					     buffer, sizeof(buffer));

		if (nbytes == 0 && input_stream_lock_eof(is))
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
		if (cmd == DECODE_COMMAND_SEEK) {
			goffset offset = (goffset)(time_to_size *
						   decoder_seek_where(decoder));
			if (input_stream_lock_seek(is, offset, SEEK_SET,
						   &error)) {
				decoder_command_finished(decoder);
			} else {
				g_warning("seeking failed: %s", error->message);
				g_error_free(error);
				decoder_seek_error(decoder);
			}

			cmd = DECODE_COMMAND_NONE;
		}
	} while (cmd == DECODE_COMMAND_NONE);
}

static const char *const pcm_mime_types[] = {
	/* for streams obtained by the cdio_paranoia input plugin */
	"audio/x-mpd-cdda-pcm",

	/* same as above, but with reverse byte order */
	"audio/x-mpd-cdda-pcm-reverse",

	nullptr
};

const struct decoder_plugin pcm_decoder_plugin = {
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
