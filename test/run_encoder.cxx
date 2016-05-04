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
#include "encoder/EncoderList.hxx"
#include "encoder/EncoderPlugin.hxx"
#include "encoder/EncoderInterface.hxx"
#include "encoder/ToOutputStream.hxx"
#include "AudioFormat.hxx"
#include "AudioParser.hxx"
#include "config/Block.hxx"
#include "fs/io/StdioOutputStream.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>

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

	ConfigBlock block;
	block.AddBlockParam("quality", "5.0", -1);

	try {
		Error error;
		const auto p_encoder = encoder_init(*plugin, block, error);
		if (p_encoder == nullptr) {
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

		auto *encoder = p_encoder->Open(audio_format, error);
		if (encoder == nullptr) {
			LogError(error, "Failed to open encoder");
			return EXIT_FAILURE;
		}

		StdioOutputStream os(stdout);

		EncoderToOutputStream(os, *encoder);

		/* do it */

		ssize_t nbytes;
		while ((nbytes = read(0, buffer, sizeof(buffer))) > 0) {
			if (!encoder->Write(buffer, nbytes, error)) {
				LogError(error, "encoder_write() failed");
				return EXIT_FAILURE;
			}

			EncoderToOutputStream(os, *encoder);
		}

		if (!encoder->End(error)) {
			LogError(error, "encoder_flush() failed");
			return EXIT_FAILURE;
		}

		EncoderToOutputStream(os, *encoder);

		delete encoder;
		p_encoder->Dispose();

		return EXIT_SUCCESS;
	} catch (const std::exception &e) {
		LogError(e);
		return EXIT_FAILURE;
	}
}
