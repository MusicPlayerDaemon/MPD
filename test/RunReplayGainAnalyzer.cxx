// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ReadFrames.hxx"
#include "pcm/ReplayGainAnalyzer.hxx"
#include "io/FileDescriptor.hxx"
#include "system/Error.hxx"
#include "util/PrintException.hxx"

#include <array>
#include <memory>

#include <stdlib.h>

int
main(int, char **) noexcept
try {
	WindowReplayGainAnalyzer a;

	constexpr std::size_t frame_size = ReplayGainAnalyzer::CHANNELS *
		sizeof(ReplayGainAnalyzer::sample_type);

	const FileDescriptor input_fd(STDIN_FILENO);

	while (true) {
		std::array<ReplayGainAnalyzer::Frame, 1024> buffer;

		size_t nbytes = ReadFrames(input_fd,
					   buffer.data(), sizeof(buffer),
					   frame_size);
		if (nbytes == 0)
			break;

		a.Process({buffer.data(), nbytes / frame_size});
	}

	a.Flush();

	printf("gain = %+.2f dB\n", (double)a.GetGain());
	printf("peak = %.6f\n", (double)a.GetPeak());

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
