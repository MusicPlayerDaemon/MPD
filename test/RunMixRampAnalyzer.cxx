// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ReadFrames.hxx"
#include "pcm/MixRampAnalyzer.hxx"
#include "io/FileDescriptor.hxx"
#include "util/PrintException.hxx"

#include <array>
#include <memory>

#include <stdio.h>
#include <stdlib.h>

int
main(int, char **) noexcept
try {
	constexpr std::size_t frame_size = sizeof(ReplayGainAnalyzer::Frame);

	const FileDescriptor input_fd(STDIN_FILENO);

	MixRampAnalyzer a;

	while (true) {
		std::array<ReplayGainAnalyzer::Frame, 4096> buffer;

		size_t nbytes = ReadFrames(input_fd,
					   buffer.data(), sizeof(buffer),
					   frame_size);
		if (nbytes == 0)
			break;

		const std::size_t n_frames = nbytes / frame_size;
		a.Process({buffer.data(), n_frames});
	}

	const auto data = a.GetResult();

	const auto total_time = a.GetTime();

	printf("MIXRAMP_START=");

	MixRampItem last{};

	for (const auto &i : data.start) {
		if (i.time >= FloatDuration{} && i != last) {
			printf("%.2f %.2f;", i.volume, i.time.count());
			last = i;
		}
	}

	printf("\n");

	printf("MIXRAMP_END=");
	last = {};
	for (const auto &i : data.end) {
		if (i.time >= FloatDuration{} && i != last) {
			printf("%.2f %.2f;", i.volume, (total_time - i.time).count());
			last = i;
		}
	}
	printf("\n");

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
