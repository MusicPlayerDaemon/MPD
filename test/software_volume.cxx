// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * This program is a command line interface to MPD's software volume
 * library (pcm_volume.c).
 *
 */

#include "pcm/Volume.hxx"
#include "pcm/AudioParser.hxx"
#include "pcm/AudioFormat.hxx"
#include "util/PrintException.hxx"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char **argv)
try {
	static std::byte buffer[4096];
	ssize_t nbytes;

	if (argc > 2) {
		fprintf(stderr, "Usage: software_volume [FORMAT] <IN >OUT\n");
		return EXIT_FAILURE;
	}

	AudioFormat audio_format(48000, SampleFormat::S16, 2);
	if (argc > 1)
		audio_format = ParseAudioFormat(argv[1], false);

	PcmVolume pv;
	const auto out_sample_format = pv.Open(audio_format.format, false);

	if (out_sample_format != audio_format.format)
		fprintf(stderr, "Converting to %s\n",
			sample_format_to_string(out_sample_format));

	while ((nbytes = read(0, buffer, sizeof(buffer))) > 0) {
		auto dest = pv.Apply({buffer, size_t(nbytes)});
		[[maybe_unused]] ssize_t ignored = write(1, dest.data(), dest.size());
	}

	pv.Close();
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
