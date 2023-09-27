// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * This program is a command line interface to MPD's PCM conversion
 * library (pcm_convert.c).
 *
 */

#include "ConfigGlue.hxx"
#include "pcm/AudioParser.hxx"
#include "pcm/AudioFormat.hxx"
#include "pcm/Convert.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "io/FileDescriptor.hxx"
#include "util/StaticFifoBuffer.hxx"
#include "cmdline/OptionDef.hxx"
#include "cmdline/OptionParser.hxx"
#include "util/PrintException.hxx"
#include "Log.hxx"
#include "LogBackend.hxx"

#include <cassert>
#include <stdexcept>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

struct CommandLine {
	AudioFormat in_audio_format, out_audio_format;

	FromNarrowPath config_path;

	bool verbose = false;
};

enum Option {
	OPTION_CONFIG,
	OPTION_VERBOSE,
};

static constexpr OptionDef option_defs[] = {
	{"config", 0, true, "Load a MPD configuration file"},
	{"verbose", 'v', false, "Verbose logging"},
};

static CommandLine
ParseCommandLine(int argc, char **argv)
{
	CommandLine c;

	OptionParser option_parser(option_defs, argc, argv);
	while (auto o = option_parser.Next()) {
		switch (Option(o.index)) {
		case OPTION_CONFIG:
			c.config_path = o.value;
			break;

		case OPTION_VERBOSE:
			c.verbose = true;
			break;
		}
	}

	auto args = option_parser.GetRemaining();
	if (args.size() != 2)
		throw std::runtime_error("Usage: run_convert IN_FORMAT OUT_FORMAT <IN >OUT");

	c.in_audio_format = ParseAudioFormat(args[0], false);
	c.out_audio_format = c.in_audio_format.WithMask(ParseAudioFormat(args[1], false));
	return c;
}

class GlobalInit {
	const ConfigData config;

public:
	explicit GlobalInit(Path config_path)
		:config(AutoLoadConfigFile(config_path))
	{
		pcm_convert_global_init(config);
	}
};

static void
RunConvert(PcmConvert &convert, size_t in_frame_size,
	   FileDescriptor in_fd, FileDescriptor out_fd)
{
	in_fd.SetBinaryMode();
	out_fd.SetBinaryMode();

	StaticFifoBuffer<std::byte, 4096> buffer;

	while (true) {
		{
			const auto dest = buffer.Write();
			assert(!dest.empty());

			ssize_t nbytes = in_fd.Read(dest);
			if (nbytes <= 0)
				break;

			buffer.Append(nbytes);
		}

		auto src = buffer.Read();
		assert(!src.empty());

		src = src.first(src.size() - src.size() % in_frame_size);
		if (src.empty())
			continue;

		buffer.Consume(src.size());

		auto output = convert.Convert(src);
		out_fd.FullWrite(output);
	}

	while (true) {
		auto output = convert.Flush();
		if (output.data() == nullptr)
			break;

		out_fd.FullWrite(output);
	}
}

int
main(int argc, char **argv)
try {
	const auto c = ParseCommandLine(argc, argv);

	SetLogThreshold(c.verbose ? LogLevel::DEBUG : LogLevel::INFO);
	const GlobalInit init(c.config_path);

	PcmConvert state(c.in_audio_format, c.out_audio_format);
	RunConvert(state, c.in_audio_format.GetFrameSize(),
		   FileDescriptor(STDIN_FILENO),
		   FileDescriptor(STDOUT_FILENO));

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
