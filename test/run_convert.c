/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "audio_parser.h"
#include "audio_format.h"
#include "pcm_convert.h"
#include "conf.h"

#include <glib.h>

#include <stddef.h>
#include <unistd.h>

const char *
config_get_string(G_GNUC_UNUSED const char *name, const char *default_value)
{
	return default_value;
}

int main(int argc, char **argv)
{
	GError *error = NULL;
	struct audio_format in_audio_format, out_audio_format;
	struct pcm_convert_state state;
	static char buffer[4096];
	const void *output;
	ssize_t nbytes;
	size_t length;

	if (argc != 3) {
		g_printerr("Usage: run_convert IN_FORMAT OUT_FORMAT <IN >OUT\n");
		return 1;
	}

	if (!audio_format_parse(&in_audio_format, argv[1],
				false, &error)) {
		g_printerr("Failed to parse audio format: %s\n",
			   error->message);
		return 1;
	}

	if (!audio_format_parse(&out_audio_format, argv[2],
				false, &error)) {
		g_printerr("Failed to parse audio format: %s\n",
			   error->message);
		return 1;
	}

	pcm_convert_init(&state);

	while ((nbytes = read(0, buffer, sizeof(buffer))) > 0) {
		output = pcm_convert(&state, &in_audio_format, buffer, nbytes,
				     &out_audio_format, &length, &error);
		if (output == NULL) {
			g_printerr("Failed to convert: %s\n", error->message);
			return 2;
		}

		write(1, output, length);
	}

	pcm_convert_deinit(&state);
}
