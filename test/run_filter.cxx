/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#include "config.h"
#include "config/Param.hxx"
#include "config/ConfigGlobal.hxx"
#include "fs/Path.hxx"
#include "AudioParser.hxx"
#include "AudioFormat.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/FilterInternal.hxx"
#include "pcm/Volume.hxx"
#include "mixer/MixerControl.hxx"
#include "util/Error.hxx"
#include "util/ConstBuffer.hxx"
#include "system/FatalError.hxx"
#include "Log.hxx"

#include <memory>

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

static PreparedFilter *
load_filter(const char *name)
{
	const auto *param = config_find_block(ConfigBlockOption::AUDIO_FILTER,
					      "name", name);
	if (param == NULL) {
		fprintf(stderr, "No such configured filter: %s\n", name);
		return nullptr;
	}

	return filter_configured_new(*param);
}

int main(int argc, char **argv)
try {
	struct audio_format_string af_string;
	char buffer[4096];

	if (argc < 3 || argc > 4) {
		fprintf(stderr, "Usage: run_filter CONFIG NAME [FORMAT] <IN\n");
		return EXIT_FAILURE;
	}

	const Path config_path = Path::FromFS(argv[1]);

	AudioFormat audio_format(44100, SampleFormat::S16, 2);

	/* read configuration file (mpd.conf) */

	config_global_init();
	ReadConfigFile(config_path);

	/* parse the audio format */

	if (argc > 3) {
		Error error;
		if (!audio_format_parse(audio_format, argv[3], false, error)) {
			LogError(error, "Failed to parse audio format");
			return EXIT_FAILURE;
		}
	}

	/* initialize the filter */

	std::unique_ptr<PreparedFilter> prepared_filter(load_filter(argv[2]));
	if (!prepared_filter)
		return EXIT_FAILURE;

	/* open the filter */

	std::unique_ptr<Filter> filter(prepared_filter->Open(audio_format));

	const AudioFormat out_audio_format = filter->GetOutAudioFormat();

	fprintf(stderr, "audio_format=%s\n",
		audio_format_to_string(out_audio_format, &af_string));

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

	config_global_finish();

	return EXIT_SUCCESS;
} catch (const std::exception &e) {
	LogError(e);
	return EXIT_FAILURE;
}
