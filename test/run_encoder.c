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

#include "config.h"
#include "encoder_list.h"
#include "encoder_plugin.h"
#include "audio_format.h"
#include "audio_parser.h"
#include "conf.h"

#include <glib.h>

#include <stddef.h>
#include <unistd.h>

static void
encoder_to_stdout(struct encoder *encoder)
{
	size_t length;
	static char buffer[32768];

	while ((length = encoder_read(encoder, buffer, sizeof(buffer))) > 0)
		write(1, buffer, length);
}

int main(int argc, char **argv)
{
	GError *error = NULL;
	struct audio_format audio_format;
	bool ret;
	const char *encoder_name;
	const struct encoder_plugin *plugin;
	struct encoder *encoder;
	struct config_param *param;
	static char buffer[32768];
	ssize_t nbytes;

	/* parse command line */

	if (argc > 3) {
		g_printerr("Usage: run_encoder [ENCODER] [FORMAT] <IN >OUT\n");
		return 1;
	}

	if (argc > 1)
		encoder_name = argv[1];
	else
		encoder_name = "vorbis";

	audio_format_init(&audio_format, 44100, 16, 2);

	/* create the encoder */

	plugin = encoder_plugin_get(encoder_name);
	if (plugin == NULL) {
		g_printerr("No such encoder: %s\n", encoder_name);
		return 1;
	}

	param = config_new_param(NULL, -1);
	config_add_block_param(param, "quality", "5.0", -1, NULL);

	encoder = encoder_init(plugin, param, &error);
	if (encoder == NULL) {
		g_printerr("Failed to initialize encoder: %s\n",
			   error->message);
		g_error_free(error);
		return 1;
	}

	/* open the encoder */

	if (argc > 2) {
		ret = audio_format_parse(&audio_format, argv[2],
					 false, &error);
		if (!ret) {
			g_printerr("Failed to parse audio format: %s\n",
				   error->message);
			g_error_free(error);
			return 1;
		}
	}

	ret = encoder_open(encoder, &audio_format, &error);
	if (encoder == NULL) {
		g_printerr("Failed to open encoder: %s\n",
			   error->message);
		g_error_free(error);
		return 1;
	}

	/* do it */

	while ((nbytes = read(0, buffer, sizeof(buffer))) > 0) {
		ret = encoder_write(encoder, buffer, nbytes, &error);
		if (!ret) {
			g_printerr("encoder_write() failed: %s\n",
				   error->message);
			g_error_free(error);
			return 1;
		}

		encoder_to_stdout(encoder);
	}

	ret = encoder_flush(encoder, &error);
	if (!ret) {
		g_printerr("encoder_flush() failed: %s\n",
			   error->message);
		g_error_free(error);
		return 1;
	}

	encoder_to_stdout(encoder);
}
