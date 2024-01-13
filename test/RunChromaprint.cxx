// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ConfigGlue.hxx"
#include "pcm/Convert.hxx"
#include "lib/chromaprint/DecoderClient.hxx"
#include "event/Thread.hxx"
#include "decoder/DecoderList.hxx"
#include "decoder/DecoderPlugin.hxx"
#include "decoder/DecoderAPI.hxx" /* for class StopDecoder */
#include "input/Init.hxx"
#include "input/InputStream.hxx"
#include "fs/NarrowPath.hxx"
#include "fs/Path.hxx"
#include "pcm/AudioFormat.hxx"
#include "cmdline/OptionDef.hxx"
#include "cmdline/OptionParser.hxx"
#include "util/PrintException.hxx"
#include "Log.hxx"
#include "LogBackend.hxx"

#include <cassert>
#include <stdexcept>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

struct CommandLine {
	const char *decoder = nullptr;
	const char *uri = nullptr;

	FromNarrowPath config_path = nullptr;

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
		throw std::runtime_error("Usage: RunChromaprint [--verbose] [--config=FILE] DECODER URI");

	c.decoder = args[0];
	c.uri = args[1];
	return c;
}

class GlobalInit {
	const ConfigData config;
	EventThread io_thread;
	const ScopeInputPluginsInit input_plugins_init;
	const ScopeDecoderPluginsInit decoder_plugins_init;

public:
	explicit GlobalInit(Path config_path)
		:config(AutoLoadConfigFile(config_path)),
		 input_plugins_init(config, io_thread.GetEventLoop()),
		 decoder_plugins_init(config)
	{
		io_thread.Start();

		pcm_convert_global_init(config);
	}
};

class MyChromaprintDecoderClient final : public ChromaprintDecoderClient {
public:
	InputStreamPtr OpenUri(const char *) override {
		throw std::runtime_error("Not implemented");
	}
};

int main(int argc, char **argv)
try {
	const auto c = ParseCommandLine(argc, argv);

	SetLogThreshold(c.verbose ? LogLevel::DEBUG : LogLevel::INFO);
	const GlobalInit init(c.config_path);

	const DecoderPlugin *plugin = decoder_plugin_from_name(c.decoder);
	if (plugin == nullptr) {
		fprintf(stderr, "No such decoder: %s\n", c.decoder);
		return EXIT_FAILURE;
	}

	MyChromaprintDecoderClient client;
	if (plugin->file_decode != nullptr) {
		try {
			plugin->FileDecode(client, FromNarrowPath(c.uri));
		} catch (StopDecoder) {
		}
	} else if (plugin->stream_decode != nullptr) {
		auto is = InputStream::OpenReady(c.uri, client.mutex);
		try {
			plugin->StreamDecode(client, *is);
		} catch (StopDecoder) {
		}
	} else {
		fprintf(stderr, "Decoder plugin is not usable\n");
		return EXIT_FAILURE;
	}

	client.Finish();
	printf("%s\n", client.GetFingerprint().c_str());
	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
