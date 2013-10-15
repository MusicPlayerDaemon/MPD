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

/*
 * This program is a command line interface to MPD's PCM conversion
 * library (pcm_convert.c).
 *
 */

#include "config.h"
#include "AudioParser.hxx"
#include "AudioFormat.hxx"
#include "pcm/PcmConvert.hxx"
#include "ConfigGlobal.hxx"
#include "util/FifoBuffer.hxx"
#include "util/Error.hxx"
#include "stdbin.h"

#include <glib.h>

#include <assert.h>
#include <stddef.h>
#include <unistd.h>

static void
my_log_func(const gchar *log_domain, gcc_unused GLogLevelFlags log_level,
	    const gchar *message, gcc_unused gpointer user_data)
{
	if (log_domain != NULL)
		g_printerr("%s: %s\n", log_domain, message);
	else
		g_printerr("%s\n", message);
}

const char *
config_get_string(gcc_unused enum ConfigOption option,
		  const char *default_value)
{
	return default_value;
}

int main(int argc, char **argv)
{
	AudioFormat in_audio_format, out_audio_format;
	const void *output;

	if (argc != 3) {
		g_printerr("Usage: run_convert IN_FORMAT OUT_FORMAT <IN >OUT\n");
		return 1;
	}

	g_log_set_default_handler(my_log_func, NULL);

	Error error;
	if (!audio_format_parse(in_audio_format, argv[1],
				false, error)) {
		g_printerr("Failed to parse audio format: %s\n",
			   error.GetMessage());
		return 1;
	}

	AudioFormat out_audio_format_mask;
	if (!audio_format_parse(out_audio_format_mask, argv[2],
				true, error)) {
		g_printerr("Failed to parse audio format: %s\n",
			   error.GetMessage());
		return 1;
	}

	out_audio_format = in_audio_format;
	out_audio_format.ApplyMask(out_audio_format_mask);

	const size_t in_frame_size = in_audio_format.GetFrameSize();

	PcmConvert state;

	FifoBuffer<uint8_t, 4096> buffer;

	while (true) {
		{
			const auto dest = buffer.Write();
			assert(!dest.IsEmpty());

			ssize_t nbytes = read(0, dest.data, dest.size);
			if (nbytes <= 0)
				break;

			buffer.Append(nbytes);
		}

		auto src = buffer.Read();
		assert(!src.IsEmpty());

		src.size -= src.size % in_frame_size;
		if (src.IsEmpty())
			continue;

		buffer.Consume(src.size);

		size_t length;
		output = state.Convert(in_audio_format, src.data, src.size,
				       out_audio_format, &length, error);
		if (output == NULL) {
			g_printerr("Failed to convert: %s\n", error.GetMessage());
			return 2;
		}

		gcc_unused ssize_t ignored = write(1, output, length);
	}

	return EXIT_SUCCESS;
}
