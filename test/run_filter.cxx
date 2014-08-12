/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "config/ConfigData.hxx"
#include "config/ConfigGlobal.hxx"
#include "fs/Path.hxx"
#include "AudioParser.hxx"
#include "AudioFormat.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/FilterInternal.hxx"
#include "pcm/Volume.hxx"
#include "mixer/MixerControl.hxx"
#include "stdbin.h"
#include "util/Error.hxx"
#include "util/ConstBuffer.hxx"
#include "system/FatalError.hxx"
#include "Log.hxx"

#ifdef HAVE_GLIB
#include <glib.h>
#endif

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

bool
mixer_set_volume(gcc_unused Mixer *mixer,
		 gcc_unused unsigned volume, gcc_unused Error &error)
{
	return true;
}

static Filter *
load_filter(const char *name)
{
	const config_param *param =
		config_find_block(CONF_AUDIO_FILTER, "name", name);
	if (param == NULL) {
		fprintf(stderr, "No such configured filter: %s\n", name);
		return nullptr;
	}

	Error error;
	Filter *filter = filter_configured_new(*param, error);
	if (filter == NULL) {
		LogError(error, "Failed to load filter");
		return NULL;
	}

	return filter;
}

int main(int argc, char **argv)
{
	struct audio_format_string af_string;
	Error error2;
	char buffer[4096];

	if (argc < 3 || argc > 4) {
		fprintf(stderr, "Usage: run_filter CONFIG NAME [FORMAT] <IN\n");
		return EXIT_FAILURE;
	}

	const Path config_path = Path::FromFS(argv[1]);

	AudioFormat audio_format(44100, SampleFormat::S16, 2);

	/* initialize GLib */

#ifdef HAVE_GLIB
#if !GLIB_CHECK_VERSION(2,32,0)
	g_thread_init(NULL);
#endif
#endif

	/* read configuration file (mpd.conf) */

	config_global_init();
	if (!ReadConfigFile(config_path, error2))
		FatalError(error2);

	/* parse the audio format */

	if (argc > 3) {
		Error error;
		if (!audio_format_parse(audio_format, argv[3], false, error)) {
			LogError(error, "Failed to parse audio format");
			return EXIT_FAILURE;
		}
	}

	/* initialize the filter */

	Filter *filter = load_filter(argv[2]);
	if (filter == NULL)
		return EXIT_FAILURE;

	/* open the filter */

	Error error;
	const AudioFormat out_audio_format = filter->Open(audio_format, error);
	if (!out_audio_format.IsDefined()) {
		LogError(error, "Failed to open filter");
		delete filter;
		return EXIT_FAILURE;
	}

	fprintf(stderr, "audio_format=%s\n",
		audio_format_to_string(out_audio_format, &af_string));

	/* play */

	while (true) {
		ssize_t nbytes;

		nbytes = read(0, buffer, sizeof(buffer));
		if (nbytes <= 0)
			break;

		auto dest = filter->FilterPCM({(const void *)buffer, (size_t)nbytes},
					      error);
		if (dest.IsNull()) {
			LogError(error, "filter/Filter failed");
			filter->Close();
			delete filter;
			return EXIT_FAILURE;
		}

		nbytes = write(1, dest.data, dest.size);
		if (nbytes < 0) {
			fprintf(stderr, "Failed to write: %s\n",
				strerror(errno));
			filter->Close();
			delete filter;
			return 1;
		}
	}

	/* cleanup and exit */

	filter->Close();
	delete filter;

	config_global_finish();

	return 0;
}
