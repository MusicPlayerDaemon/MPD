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
#include "pcm/MixRampAnalyzer.hxx"
#include "io/FileDescriptor.hxx"
#include "util/ConstBuffer.hxx"
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
