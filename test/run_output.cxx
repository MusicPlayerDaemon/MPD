/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "output/Internal.hxx"
#include "output/OutputPlugin.hxx"
#include "output/Client.hxx"
#include "config/Param.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigOption.hxx"
#include "Idle.hxx"
#include "Main.hxx"
#include "event/Thread.hxx"
#include "fs/Path.hxx"
#include "AudioParser.hxx"
#include "ReplayGainConfig.hxx"
#include "pcm/PcmConvert.hxx"
#include "filter/FilterRegistry.hxx"
#include "util/StringBuffer.hxx"
#include "util/RuntimeError.hxx"
#include "util/ScopeExit.hxx"
#include "Log.hxx"

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

class DummyAudioOutputClient final : public AudioOutputClient {
public:
	/* virtual methods from AudioOutputClient */
	void ChunksConsumed() override {
	}

	void ApplyEnabled() override {
	}
};

const FilterPlugin *
filter_plugin_by_name(gcc_unused const char *name) noexcept
{
	assert(false);
	return NULL;
}

static AudioOutput *
load_audio_output(EventLoop &event_loop, AudioOutputClient &client,
		  const char *name)
{
	const auto *param = config_find_block(ConfigBlockOption::AUDIO_OUTPUT,
					      "name", name);
	if (param == NULL)
		throw FormatRuntimeError("No such configured audio output: %s\n",
					 name);

	return audio_output_new(event_loop, ReplayGainConfig(), *param,
				*(MixerListener *)nullptr,
				client);
}

static void
run_output(AudioOutput &ao, AudioFormat audio_format)
{
	/* open the audio output */

	ao_plugin_enable(ao);
	AtScopeExit(&ao) { ao_plugin_disable(ao); };

	ao_plugin_open(ao, audio_format);
	AtScopeExit(&ao) { ao_plugin_close(ao); };

	fprintf(stderr, "audio_format=%s\n",
		ToString(audio_format).c_str());

	size_t frame_size = audio_format.GetFrameSize();

	/* play */

	size_t length = 0;
	char buffer[4096];
	while (true) {
		if (length < sizeof(buffer)) {
			ssize_t nbytes = read(0, buffer + length,
					      sizeof(buffer) - length);
			if (nbytes <= 0)
				break;

			length += (size_t)nbytes;
		}

		size_t play_length = (length / frame_size) * frame_size;
		if (play_length > 0) {
			size_t consumed = ao_plugin_play(ao,
							 buffer, play_length);

			assert(consumed <= length);
			assert(consumed % frame_size == 0);

			length -= consumed;
			memmove(buffer, buffer + consumed, length);
		}
	}
}

int main(int argc, char **argv)
try {
	if (argc < 3 || argc > 4) {
		fprintf(stderr, "Usage: run_output CONFIG NAME [FORMAT] <IN\n");
		return EXIT_FAILURE;
	}

	const Path config_path = Path::FromFS(argv[1]);

	AudioFormat audio_format(44100, SampleFormat::S16, 2);

	/* read configuration file (mpd.conf) */

	config_global_init();
	ReadConfigFile(config_path);

	EventThread io_thread;
	io_thread.Start();

	/* initialize the audio output */

	DummyAudioOutputClient client;
	AudioOutput *ao = load_audio_output(io_thread.GetEventLoop(), client,
					    argv[2]);

	/* parse the audio format */

	if (argc > 3)
		audio_format = ParseAudioFormat(argv[3], false);

	/* do it */

	run_output(*ao, audio_format);

	/* cleanup and exit */

	audio_output_free(ao);

	config_global_finish();

	return EXIT_SUCCESS;
 } catch (const std::exception &e) {
	LogError(e);
	return EXIT_FAILURE;
 }
