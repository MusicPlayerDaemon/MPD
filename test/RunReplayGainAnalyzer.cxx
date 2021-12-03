/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "ReadFrames.hxx"
#include "pcm/ReplayGainAnalyzer.hxx"
#include "io/FileDescriptor.hxx"
#include "system/Error.hxx"
#include "util/ConstBuffer.hxx"
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
