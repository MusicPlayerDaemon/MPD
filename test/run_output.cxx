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
#include "output/Internal.hxx"
#include "output/OutputPlugin.hxx"
#include "config/ConfigData.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigOption.hxx"
#include "Idle.hxx"
#include "Main.hxx"
#include "event/Loop.hxx"
#include "ScopeIOThread.hxx"
#include "fs/Path.hxx"
#include "AudioParser.hxx"
#include "pcm/PcmConvert.hxx"
#include "filter/FilterRegistry.hxx"
#include "PlayerControl.hxx"
#include "stdbin.h"
#include "util/Error.hxx"
#include "Log.hxx"

#ifdef HAVE_GLIB
#include <glib.h>
#endif

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

const struct filter_plugin *
filter_plugin_by_name(gcc_unused const char *name)
{
	assert(false);
	return NULL;
}

PlayerControl::PlayerControl(PlayerListener &_listener,
			     MultipleOutputs &_outputs,
			     unsigned _buffer_chunks,
			     unsigned _buffered_before_play)
	:listener(_listener), outputs(_outputs),
	 buffer_chunks(_buffer_chunks),
	 buffered_before_play(_buffered_before_play) {}
PlayerControl::~PlayerControl() {}

static AudioOutput *
load_audio_output(EventLoop &event_loop, const char *name)
{
	const config_param *param =
		config_find_block(CONF_AUDIO_OUTPUT, "name", name);
	if (param == NULL) {
		fprintf(stderr, "No such configured audio output: %s\n", name);
		return nullptr;
	}

	static struct PlayerControl dummy_player_control(*(PlayerListener *)nullptr,
							 *(MultipleOutputs *)nullptr,
							 32, 4);

	Error error;
	AudioOutput *ao =
		audio_output_new(event_loop, *param,
				 *(MixerListener *)nullptr,
				 dummy_player_control,
				 error);
	if (ao == nullptr)
		LogError(error);

	return ao;
}

static bool
run_output(AudioOutput *ao, AudioFormat audio_format)
{
	/* open the audio output */

	Error error;
	if (!ao_plugin_enable(ao, error)) {
		LogError(error, "Failed to enable audio output");
		return false;
	}

	if (!ao_plugin_open(ao, audio_format, error)) {
		ao_plugin_disable(ao);
		LogError(error, "Failed to open audio output");
		return false;
	}

	struct audio_format_string af_string;
	fprintf(stderr, "audio_format=%s\n",
		audio_format_to_string(audio_format, &af_string));

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
							 buffer, play_length,
							 error);
			if (consumed == 0) {
				ao_plugin_close(ao);
				ao_plugin_disable(ao);
				LogError(error, "Failed to play");
				return false;
			}

			assert(consumed <= length);
			assert(consumed % frame_size == 0);

			length -= consumed;
			memmove(buffer, buffer + consumed, length);
		}
	}

	ao_plugin_close(ao);
	ao_plugin_disable(ao);
	return true;
}

int main(int argc, char **argv)
{
	Error error;

	if (argc < 3 || argc > 4) {
		fprintf(stderr, "Usage: run_output CONFIG NAME [FORMAT] <IN\n");
		return EXIT_FAILURE;
	}

	const Path config_path = Path::FromFS(argv[1]);

	AudioFormat audio_format(44100, SampleFormat::S16, 2);

#ifdef HAVE_GLIB
#if !GLIB_CHECK_VERSION(2,32,0)
	g_thread_init(NULL);
#endif
#endif

	/* read configuration file (mpd.conf) */

	config_global_init();
	if (!ReadConfigFile(config_path, error)) {
		LogError(error);
		return EXIT_FAILURE;
	}

	EventLoop event_loop;

	const ScopeIOThread io_thread;

	/* initialize the audio output */

	AudioOutput *ao = load_audio_output(event_loop, argv[2]);
	if (ao == NULL)
		return 1;

	/* parse the audio format */

	if (argc > 3) {
		if (!audio_format_parse(audio_format, argv[3], false, error)) {
			LogError(error, "Failed to parse audio format");
			return EXIT_FAILURE;
		}
	}

	/* do it */

	bool success = run_output(ao, audio_format);

	/* cleanup and exit */

	audio_output_free(ao);

	config_global_finish();

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
