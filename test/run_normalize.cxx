// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * This program is a command line interface to MPD's normalize library
 * (based on AudioCompress).
 *
 */

#include "pcm/Normalizer.hxx"
#include "pcm/AudioParser.hxx"
#include "pcm/AudioFormat.hxx"
#include "util/PrintException.hxx"
#include "util/SpanCast.hxx"

#include <stdexcept>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv)
try {
	if (argc > 2) {
		fprintf(stderr, "Usage: run_normalize [FORMAT] <IN >OUT\n");
		return 1;
	}

	AudioFormat audio_format(48000, SampleFormat::S16, 2);
	if (argc > 1)
		audio_format = ParseAudioFormat(argv[1], false);

	PcmNormalizer normalizer;

	static std::byte buffer[4096];
	ssize_t nbytes;
	while ((nbytes = read(0, buffer, sizeof(buffer))) > 0) {
		static int16_t dest[2048];
		normalizer.ProcessS16(dest, FromBytesStrict<const int16_t>(std::span{buffer}.first(nbytes)));
		[[maybe_unused]] ssize_t ignored = write(1, dest, nbytes);
	}

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
