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
 * This program is a command line interface to MPD's software volume
 * library (pcm_volume.c).
 *
 */

#include "config.h"
#include "pcm_volume.h"
#include "audio_parser.h"
#include "audio_format.h"

#include <glib.h>

#include <stddef.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	GError *error = NULL;
	struct audio_format audio_format;
	bool ret;
	static char buffer[4096];
	ssize_t nbytes;

	if (argc > 2) {
		g_printerr("Usage: software_volume [FORMAT] <IN >OUT\n");
		return 1;
	}

	if (argc > 1) {
		ret = audio_format_parse(&audio_format, argv[1],
					 false, &error);
		if (!ret) {
			g_printerr("Failed to parse audio format: %s\n",
				   error->message);
			return 1;
		}
	} else
		audio_format_init(&audio_format, 48000, 16, 2);

	while ((nbytes = read(0, buffer, sizeof(buffer))) > 0) {
		if (!pcm_volume(buffer, nbytes, &audio_format,
				PCM_VOLUME_1 / 2)) {
			g_printerr("pcm_volume() has failed\n");
			return 2;
		}

		write(1, buffer, nbytes);
	}
}
