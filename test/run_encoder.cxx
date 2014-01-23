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
#include "encoder/EncoderList.hxx"
#include "encoder/EncoderPlugin.hxx"
#include "AudioFormat.hxx"
#include "AudioParser.hxx"
#include "config/ConfigData.hxx"
#include "util/Error.hxx"
#include "Log.hxx"
#include "stdbin.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>

static void
encoder_to_stdout(Encoder &encoder)
{
	size_t length;
	static char buffer[32768];

	while ((length = encoder_read(&encoder, buffer, sizeof(buffer))) > 0) {
		gcc_unused ssize_t ignored = write(1, buffer, length);
	}
}

int main(int argc, char **argv)
{
	const char *encoder_name;
	static char buffer[32768];

	/* parse command line */

	if (argc > 3) {
		fprintf(stderr,
			"Usage: run_encoder [ENCODER] [FORMAT] <IN >OUT\n");
		return EXIT_FAILURE;
	}

	if (argc > 1)
		encoder_name = argv[1];
	else
		encoder_name = "vorbis";

	/* create the encoder */

	const auto plugin = encoder_plugin_get(encoder_name);
	if (plugin == NULL) {
		fprintf(stderr, "No such encoder: %s\n", encoder_name);
		return EXIT_FAILURE;
	}

	config_param param;
	param.AddBlockParam("quality", "5.0", -1);

	Error error;
	const auto encoder = encoder_init(*plugin, param, error);
	if (encoder == NULL) {
		LogError(error, "Failed to initialize encoder");
		return EXIT_FAILURE;
	}

	/* open the encoder */

	AudioFormat audio_format(44100, SampleFormat::S16, 2);
	if (argc > 2) {
		if (!audio_format_parse(audio_format, argv[2], false, error)) {
			LogError(error, "Failed to parse audio format");
			return EXIT_FAILURE;
		}
	}

	if (!encoder_open(encoder, audio_format, error)) {
		LogError(error, "Failed to open encoder");
		return EXIT_FAILURE;
	}

	encoder_to_stdout(*encoder);

	/* do it */

	ssize_t nbytes;
	while ((nbytes = read(0, buffer, sizeof(buffer))) > 0) {
		if (!encoder_write(encoder, buffer, nbytes, error)) {
			LogError(error, "encoder_write() failed");
			return EXIT_FAILURE;
		}

		encoder_to_stdout(*encoder);
	}

	if (!encoder_end(encoder, error)) {
		LogError(error, "encoder_flush() failed");
		return EXIT_FAILURE;
	}

	encoder_to_stdout(*encoder);

	encoder_close(encoder);
	encoder_finish(encoder);
}
