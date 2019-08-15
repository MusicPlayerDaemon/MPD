/*
 * Copyright 2003-2019 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will  useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "output/Interface.hxx"
#include "output/Registry.hxx"
#include "output/OutputPlugin.hxx"
#include "ConfigGlue.hxx"
#include "event/Thread.hxx"
#include "fs/Path.hxx"
#include "AudioParser.hxx"
#include "AudioFormat.hxx"
#include "util/StringBuffer.hxx"
#include "util/RuntimeError.hxx"
#include "util/ScopeExit.hxx"
#include "util/PrintException.hxx"

#include <memory>

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

static std::unique_ptr<AudioOutput>
LoadAudioOutput(const ConfigData &config, EventLoop &event_loop,
		const char *name)
{
	const auto *block = config.FindBlock(ConfigBlockOption::AUDIO_OUTPUT,
					     "name", name);
	if (block == nullptr)
		throw FormatRuntimeError("No such configured audio output: %s",
					 name);

	const char *plugin_name = block->GetBlockValue("type");
	if (plugin_name == nullptr)
		throw std::runtime_error("Missing \"type\" configuration");

	const auto *plugin = AudioOutputPlugin_get(plugin_name);
	if (plugin == nullptr)
		throw FormatRuntimeError("No such audio output plugin: %s",
					 plugin_name);

	return std::unique_ptr<AudioOutput>(ao_plugin_init(event_loop, *plugin,
							   *block));
}

static void
run_output(AudioOutput &ao, AudioFormat audio_format)
{
	/* open the audio output */

	ao.Enable();
	AtScopeExit(&ao) { ao.Disable(); };

	ao.Open(audio_format);
	AtScopeExit(&ao) { ao.Close(); };

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
			size_t consumed = ao.Play(buffer, play_length);

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

	const auto config = AutoLoadConfigFile(config_path);

	EventThread io_thread;
	io_thread.Start();

	/* initialize the audio output */

	auto ao = LoadAudioOutput(config, io_thread.GetEventLoop(), argv[2]);

	/* parse the audio format */

	if (argc > 3)
		audio_format = ParseAudioFormat(argv[3], false);

	/* do it */

	run_output(*ao, audio_format);

	/* cleanup and exit */

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
