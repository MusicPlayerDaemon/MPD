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

#include "ConfigGlue.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "AudioParser.hxx"
#include "AudioFormat.hxx"
#include "filter/LoadOne.hxx"
#include "filter/Filter.hxx"
#include "filter/Prepared.hxx"
#include "pcm/Volume.hxx"
#include "mixer/MixerControl.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StringBuffer.hxx"
#include "util/RuntimeError.hxx"
#include "util/PrintException.hxx"

#include <memory>
#include <stdexcept>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

void
mixer_set_volume(gcc_unused Mixer *mixer,
		 gcc_unused unsigned volume)
{
}

static std::unique_ptr<PreparedFilter>
LoadFilter(const ConfigData &config, const char *name)
{
	const auto *param = config.FindBlock(ConfigBlockOption::AUDIO_FILTER,
					     "name", name);
	if (param == NULL)
		throw FormatRuntimeError("No such configured filter: %s",
					 name);

	return filter_configured_new(*param);
}

int main(int argc, char **argv)
try {
	char buffer[4096];

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

	/* initialize the filter */

	auto prepared_filter = LoadFilter(config, argv[2]);

	/* open the filter */

	auto filter = prepared_filter->Open(audio_format);

	const AudioFormat out_audio_format = filter->GetOutAudioFormat();

	fprintf(stderr, "audio_format=%s\n",
		ToString(out_audio_format).c_str());

	/* play */

	while (true) {
		ssize_t nbytes;

		nbytes = read(0, buffer, sizeof(buffer));
		if (nbytes <= 0)
			break;

		auto dest = filter->FilterPCM({(const void *)buffer, (size_t)nbytes});

		nbytes = write(1, dest.data, dest.size);
		if (nbytes < 0) {
			fprintf(stderr, "Failed to write: %s\n",
				strerror(errno));
			return 1;
		}
	}

	/* cleanup and exit */

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
