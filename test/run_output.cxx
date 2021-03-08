/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
#include "fs/NarrowPath.hxx"
#include "pcm/AudioParser.hxx"
#include "pcm/AudioFormat.hxx"
#include "util/OptionDef.hxx"
#include "util/OptionParser.hxx"
#include "util/StringBuffer.hxx"
#include "util/RuntimeError.hxx"
#include "util/ScopeExit.hxx"
#include "util/StaticFifoBuffer.hxx"
#include "util/PrintException.hxx"
#include "LogBackend.hxx"

#include <cassert>
#include <memory>

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

struct CommandLine {
	FromNarrowPath config_path;

	const char *output_name = nullptr;

	AudioFormat audio_format{44100, SampleFormat::S16, 2};

	bool verbose = false;
};

enum Option {
	OPTION_VERBOSE,
};

static constexpr OptionDef option_defs[] = {
	{"verbose", 'v', false, "Verbose logging"},
};

static CommandLine
ParseCommandLine(int argc, char **argv)
{
	CommandLine c;

	OptionParser option_parser(option_defs, argc, argv);
	while (auto o = option_parser.Next()) {
		switch (Option(o.index)) {
		case OPTION_VERBOSE:
			c.verbose = true;
			break;
		}
	}

	auto args = option_parser.GetRemaining();
	if (args.size < 2 || args.size > 3)
		throw std::runtime_error("Usage: run_output CONFIG NAME [FORMAT] <IN");

	c.config_path = args[0];
	c.output_name = args[1];

	if (args.size > 2)
		c.audio_format = ParseAudioFormat(args[2], false);

	return c;
}

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
#include "util/OptionDef.hxx"
#include "util/OptionParser.hxx"

	return std::unique_ptr<AudioOutput>(ao_plugin_init(event_loop, *plugin,
							   *block));
}

static void
RunOutput(AudioOutput &ao, AudioFormat audio_format,
	  FileDescriptor in_fd)
{
	in_fd.SetBinaryMode();

	/* open the audio output */

	ao.Enable();
	AtScopeExit(&ao) { ao.Disable(); };

	ao.Open(audio_format);
	AtScopeExit(&ao) { ao.Close(); };

	fprintf(stderr, "audio_format=%s\n",
		ToString(audio_format).c_str());

	const size_t in_frame_size = audio_format.GetFrameSize();

	/* play */

	StaticFifoBuffer<std::byte, 4096> buffer;

	while (true) {
		{
			const auto dest = buffer.Write();
			assert(!dest.empty());

			ssize_t nbytes = in_fd.Read(dest.data, dest.size);
			if (nbytes <= 0)
				break;

			buffer.Append(nbytes);
		}

		auto src = buffer.Read();
		assert(!src.empty());

		src.size -= src.size % in_frame_size;
		if (src.empty())
			continue;

		size_t consumed = ao.Play(src.data, src.size);

		assert(consumed <= src.size);
		assert(consumed % in_frame_size == 0);

		buffer.Consume(consumed);
	}

	ao.Drain();
}

int main(int argc, char **argv)
try {
	const auto c = ParseCommandLine(argc, argv);
	SetLogThreshold(c.verbose ? LogLevel::DEBUG : LogLevel::INFO);

	/* read configuration file (mpd.conf) */

	const auto config = AutoLoadConfigFile(c.config_path);

	EventThread io_thread;
	io_thread.Start();

	/* initialize the audio output */

	auto ao = LoadAudioOutput(config, io_thread.GetEventLoop(),
				  c.output_name);

	/* do it */

	RunOutput(*ao, c.audio_format, FileDescriptor(STDIN_FILENO));

	/* cleanup and exit */

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
