// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * This program is a command line interface to MPD's normalize library
 * (based on AudioCompress).
 *
 */

#include "pcm/AudioCompress/compress.h"
#include "pcm/AudioParser.hxx"
#include "pcm/AudioFormat.hxx"
#include "util/PrintException.hxx"

#include <stdexcept>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv)
try {
	struct Compressor *compressor;
	static char buffer[4096];
	ssize_t nbytes;

	if (argc > 2) {
		fprintf(stderr, "Usage: run_normalize [FORMAT] <IN >OUT\n");
		return 1;
	}

	AudioFormat audio_format(48000, SampleFormat::S16, 2);
	if (argc > 1)
		audio_format = ParseAudioFormat(argv[1], false);

	compressor = Compressor_new(0);

	while ((nbytes = read(0, buffer, sizeof(buffer))) > 0) {
		Compressor_Process_int16(compressor,
					 (int16_t *)buffer, nbytes / 2);

		[[maybe_unused]] ssize_t ignored = write(1, buffer, nbytes);
	}

	Compressor_delete(compressor);
	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
