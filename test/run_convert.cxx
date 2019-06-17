/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

/*
 * This program is a command line interface to MPD's PCM conversion
 * library (pcm_convert.c).
 *
 */

#include "AudioParser.hxx"
#include "AudioFormat.hxx"
#include "pcm/Convert.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StaticFifoBuffer.hxx"
#include "util/PrintException.hxx"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int
main(int argc, char **argv)
try {
	if (argc != 3) {
		fprintf(stderr,
			"Usage: run_convert IN_FORMAT OUT_FORMAT <IN >OUT\n");
		return 1;
	}

	const auto in_audio_format = ParseAudioFormat(argv[1], false);
	const auto out_audio_format_mask = ParseAudioFormat(argv[2], false);

	const auto out_audio_format =
		in_audio_format.WithMask(out_audio_format_mask);

	const size_t in_frame_size = in_audio_format.GetFrameSize();

	PcmConvert state(in_audio_format, out_audio_format);

	StaticFifoBuffer<uint8_t, 4096> buffer;

	while (true) {
		{
			const auto dest = buffer.Write();
			assert(!dest.empty());

			ssize_t nbytes = read(0, dest.data, dest.size);
			if (nbytes <= 0)
				break;

			buffer.Append(nbytes);
		}

		auto src = buffer.Read();
		assert(!src.empty());

		src.size -= src.size % in_frame_size;
		if (src.empty())
			continue;

		buffer.Consume(src.size);

		auto output = state.Convert({src.data, src.size});

		gcc_unused ssize_t ignored = write(1, output.data,
						   output.size);
	}

	while (true) {
		auto output = state.Flush();
		if (output.IsNull())
			break;

		gcc_unused ssize_t ignored = write(1, output.data,
						   output.size);
	}

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
