// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "ConfigGlue.hxx"
#include "event/Thread.hxx"
#include "decoder/DecoderList.hxx"
#include "decoder/DecoderPlugin.hxx"
#include "input/Init.hxx"
#include "input/InputStream.hxx"
#include "tag/Handler.hxx"
#include "tag/Generic.hxx"
#include "tag/Names.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "pcm/AudioFormat.hxx"
#include "lib/fmt/AudioFormatFormatter.hxx"
#include "cmdline/OptionDef.hxx"
#include "cmdline/OptionParser.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringBuffer.hxx"
#include "util/PrintException.hxx"
#include "Log.hxx"
#include "LogBackend.hxx"

#include <fmt/core.h>

#include <cassert>
#include <stdexcept>

#include <unistd.h>
#include <stdlib.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

struct CommandLine {
	const char *decoder = nullptr;
	const char *uri = nullptr;

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
		throw std::runtime_error("Usage: read_tags [--verbose] DECODER URI");

	c.decoder = args[0];
	c.uri = args[1];
	return c;
}

class GlobalInit {
	const ConfigData config;
	EventThread io_thread;
	const ScopeInputPluginsInit input_plugins_init{config, io_thread.GetEventLoop()};
	const ScopeDecoderPluginsInit decoder_plugins_init{config};

public:
	explicit GlobalInit(Path config_path)
		:config(AutoLoadConfigFile(config_path))
	{
		io_thread.Start();
	}
};

class DumpTagHandler final : public NullTagHandler {
	bool empty = true;

public:
	DumpTagHandler() noexcept
		:NullTagHandler(WANT_DURATION|WANT_TAG|WANT_PAIR|WANT_PICTURE) {}

	bool IsEmpty() const noexcept {
		return empty;
	}

	void OnDuration(SongTime duration) noexcept override {
		fmt::print("duration={}\n", duration.ToDoubleS());
	}

	void OnTag(TagType type, std::string_view value) noexcept override {
		fmt::print("[{}]={:?}\n", tag_item_names[type], value);
		empty = false;
	}

	void OnPair(std::string_view key, std::string_view value) noexcept override {
		fmt::print("{:?}={:?}\n", key, value);
	}

	void OnAudioFormat(AudioFormat af) noexcept override {
		fmt::print("{}\n", af);
	}

	void OnPicture(const char *mime_type,
		       std::span<const std::byte> buffer) noexcept override {
		fmt::print("picture mime={:?} size={}\n",
			   mime_type, buffer.size());
	}
};

int main(int argc, char **argv)
try {
#ifdef HAVE_LOCALE_H
	/* initialize locale */
	setlocale(LC_CTYPE,"");
#endif

	const auto c = ParseCommandLine(argc, argv);

	SetLogThreshold(c.verbose ? LogLevel::DEBUG : LogLevel::INFO);
	const GlobalInit init{c.config_path};

	auto *plugin = decoder_plugin_from_name(c.decoder);
	if (plugin == nullptr) {
		fmt::print(stderr, "No such decoder: {:?}\n", c.decoder);
		return EXIT_FAILURE;
	}

	DumpTagHandler h;
	bool success;
	try {
		success = plugin->ScanFile(FromNarrowPath(c.uri), h);
	} catch (...) {
		PrintException(std::current_exception());
		success = false;
	}

	Mutex mutex;
	InputStreamPtr is;

	if (!success && plugin->scan_stream != nullptr) {
		is = InputStream::OpenReady(c.uri, mutex);
		success = plugin->ScanStream(*is, h);
	}

	if (!success) {
		fmt::print(stderr, "Failed to read tags\n");
		return EXIT_FAILURE;
	}

	if (h.IsEmpty()) {
		if (is)
			ScanGenericTags(*is, h);
		else
			ScanGenericTags(FromNarrowPath(c.uri), h);
	}

	return 0;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
