// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ConfigGlue.hxx"
#include "ReadFrames.hxx"
#include "cmdline/OptionDef.hxx"
#include "cmdline/OptionParser.hxx"
#include "lib/fmt/AudioFormatFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "filter/LoadOne.hxx"
#include "filter/Filter.hxx"
#include "filter/Prepared.hxx"
#include "pcm/AudioParser.hxx"
#include "pcm/AudioFormat.hxx"
#include "pcm/Volume.hxx"
#include "mixer/Control.hxx"
#include "system/Error.hxx"
#include "io/FileDescriptor.hxx"
#include "util/StringBuffer.hxx"
#include "util/PrintException.hxx"
#include "LogBackend.hxx"

#include <cassert>
#include <memory>
#include <stdexcept>

#include <string.h>
#include <stdlib.h>

struct CommandLine {
	FromNarrowPath config_path;

	const char *filter_name = nullptr;

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
	if (args.size() < 2 || args.size() > 3)
		throw std::runtime_error("Usage: run_filter CONFIG NAME [FORMAT] <IN");

	c.config_path = args[0];
	c.filter_name = args[1];

	if (args.size() > 2)
		c.audio_format = ParseAudioFormat(args[2], false);

	return c;
}

static std::unique_ptr<PreparedFilter>
LoadFilter(const ConfigData &config, const char *name)
{
	const auto *param = config.FindBlock(ConfigBlockOption::AUDIO_FILTER,
					     "name", name);
	if (param == nullptr)
		throw FmtRuntimeError("No such configured filter: {}",
				      name);

	return filter_configured_new(*param);
}

int main(int argc, char **argv)
try {
	const auto c = ParseCommandLine(argc, argv);
	SetLogThreshold(c.verbose ? LogLevel::DEBUG : LogLevel::INFO);

	/* read configuration file (mpd.conf) */

	const auto config = AutoLoadConfigFile(c.config_path);

	auto audio_format = c.audio_format;
	const size_t in_frame_size = audio_format.GetFrameSize();

	/* initialize the filter */

	auto prepared_filter = LoadFilter(config, argv[2]);

	/* open the filter */

	auto filter = prepared_filter->Open(audio_format);

	const AudioFormat out_audio_format = filter->GetOutAudioFormat();
	fmt::print(stderr, "audio_format={}\n", out_audio_format);

	/* play */

	FileDescriptor input_fd(STDIN_FILENO);
	FileDescriptor output_fd(STDOUT_FILENO);

	while (true) {
		std::byte buffer[4096];

		ssize_t nbytes = ReadFrames(input_fd, buffer, sizeof(buffer),
					    in_frame_size);
		if (nbytes == 0)
			break;

		for (auto dest = filter->FilterPCM(std::span{buffer}.first(nbytes));
		     !dest.empty(); dest = filter->ReadMore())
			output_fd.FullWrite(dest);
	}

	while (true) {
		auto dest = filter->Flush();
		if (dest.empty())
			break;
		output_fd.FullWrite(dest);
	}

	/* cleanup and exit */

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
