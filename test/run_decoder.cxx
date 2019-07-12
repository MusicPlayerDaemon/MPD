/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "ConfigGlue.hxx"
#include "event/Thread.hxx"
#include "decoder/DecoderList.hxx"
#include "decoder/DecoderPlugin.hxx"
#include "decoder/DecoderAPI.hxx" /* for class StopDecoder */
#include "DumpDecoderClient.hxx"
#include "input/Init.hxx"
#include "input/InputStream.hxx"
#include "fs/Path.hxx"
#include "AudioFormat.hxx"
#include "util/OptionDef.hxx"
#include "util/OptionParser.hxx"
#include "util/PrintException.hxx"
#include "Log.hxx"
#include "LogBackend.hxx"

#include <stdexcept>

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

struct CommandLine {
	const char *decoder = nullptr;
	const char *uri = nullptr;

	Path config_path = nullptr;

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
			c.config_path = Path::FromFS(o.value);
			break;

		case OPTION_VERBOSE:
			c.verbose = true;
			break;
		}
	}

	auto args = option_parser.GetRemaining();
	if (args.size != 2)
		throw std::runtime_error("Usage: run_decoder [--verbose] [--config=FILE] DECODER URI");

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

	DumpDecoderClient client;
	if (plugin->file_decode != nullptr) {
		try {
			plugin->FileDecode(client, Path::FromFS(c.uri));
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

	if (!client.IsInitialized()) {
		fprintf(stderr, "Decoding failed\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
