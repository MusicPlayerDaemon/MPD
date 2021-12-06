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

#include "ConfigGlue.hxx"
#include "ReadFrames.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "filter/LoadOne.hxx"
#include "filter/Filter.hxx"
#include "filter/Prepared.hxx"
#include "pcm/AudioParser.hxx"
#include "pcm/AudioFormat.hxx"
#include "pcm/Volume.hxx"
#include "mixer/MixerControl.hxx"
#include "system/Error.hxx"
#include "io/FileDescriptor.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StringBuffer.hxx"
#include "util/RuntimeError.hxx"
#include "util/PrintException.hxx"

#include <cassert>
#include <memory>
#include <stdexcept>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void
mixer_set_volume([[maybe_unused]] Mixer *mixer,
		 [[maybe_unused]] unsigned volume)
{
}

static std::unique_ptr<PreparedFilter>
LoadFilter(const ConfigData &config, const char *name)
{
	const auto *param = config.FindBlock(ConfigBlockOption::AUDIO_FILTER,
					     "name", name);
	if (param == nullptr)
		throw FormatRuntimeError("No such configured filter: %s",
					 name);

	return filter_configured_new(*param);
}

int main(int argc, char **argv)
try {
	if (argc < 3 || argc > 4) {
		fprintf(stderr, "Usage: run_filter CONFIG NAME [FORMAT] <IN\n");
		return EXIT_FAILURE;
	}

	const FromNarrowPath config_path = argv[1];

	AudioFormat audio_format(44100, SampleFormat::S16, 2);

	/* read configuration file (mpd.conf) */

	const auto config = AutoLoadConfigFile(config_path);

	/* parse the audio format */

	if (argc > 3)
		audio_format = ParseAudioFormat(argv[3], false);

	const size_t in_frame_size = audio_format.GetFrameSize();

	/* initialize the filter */

	auto prepared_filter = LoadFilter(config, argv[2]);

	/* open the filter */

	auto filter = prepared_filter->Open(audio_format);

	const AudioFormat out_audio_format = filter->GetOutAudioFormat();

	fprintf(stderr, "audio_format=%s\n",
		ToString(out_audio_format).c_str());

	/* play */

	FileDescriptor input_fd(STDIN_FILENO);
	FileDescriptor output_fd(STDOUT_FILENO);

	while (true) {
		char buffer[4096];

		ssize_t nbytes = ReadFrames(input_fd, buffer, sizeof(buffer),
					    in_frame_size);
		if (nbytes == 0)
			break;

		auto dest = filter->FilterPCM({(const void *)buffer, (size_t)nbytes});
		output_fd.FullWrite(dest.data, dest.size);
	}

	while (true) {
		auto dest = filter->Flush();
		if (dest.IsNull())
			break;
		output_fd.FullWrite(dest.data, dest.size);
	}

	/* cleanup and exit */

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
