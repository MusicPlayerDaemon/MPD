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
#include "TagSave.hxx"
#include "tag/Tag.hxx"
#include "config/ConfigGlobal.hxx"
#include "input/InputStream.hxx"
#include "input/Init.hxx"
#include "event/Thread.hxx"
#include "thread/Cond.hxx"
#include "Log.hxx"
#include "LogBackend.hxx"
#include "fs/Path.hxx"
#include "fs/io/BufferedOutputStream.hxx"
#include "fs/io/StdioOutputStream.hxx"
#include "util/ConstBuffer.hxx"
#include "util/OptionDef.hxx"
#include "util/OptionParser.hxx"

#ifdef ENABLE_ARCHIVE
#include "archive/ArchiveList.hxx"
#endif

#include <stdexcept>

#include <unistd.h>
#include <stdlib.h>

struct CommandLine {
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
	if (args.size != 1)
		throw std::runtime_error("Usage: run_input [--verbose] [--config=FILE] URI");

	c.uri = args.front();
	return c;
}

class GlobalInit {
	EventThread io_thread;

public:
	GlobalInit(Path config_path, bool verbose) {
		SetLogThreshold(verbose ? LogLevel::DEBUG : LogLevel::INFO);

		io_thread.Start();
		config_global_init();

		if (!config_path.IsNull())
			ReadConfigFile(config_path);

#ifdef ENABLE_ARCHIVE
		archive_plugin_init_all();
#endif
		input_stream_global_init(io_thread.GetEventLoop());
	}

	~GlobalInit() {
		input_stream_global_finish();
#ifdef ENABLE_ARCHIVE
		archive_plugin_deinit_all();
#endif
		config_global_finish();
	}
};

static void
tag_save(FILE *file, const Tag &tag)
{
	StdioOutputStream sos(file);
	BufferedOutputStream bos(sos);
	tag_save(bos, tag);
	bos.Flush();
}

static int
dump_input_stream(InputStream *is)
{
	const std::lock_guard<Mutex> protect(is->mutex);

	/* print meta data */

	if (is->HasMimeType())
		fprintf(stderr, "MIME type: %s\n", is->GetMimeType());

	/* read data and tags from the stream */

	while (!is->IsEOF()) {
		{
			auto tag = is->ReadTag();
			if (tag) {
				fprintf(stderr, "Received a tag:\n");
				tag_save(stderr, *tag);
			}
		}

		char buffer[4096];
		size_t num_read = is->Read(buffer, sizeof(buffer));
		if (num_read == 0)
			break;

		ssize_t num_written = write(1, buffer, num_read);
		if (num_written <= 0)
			break;
	}

	is->Check();

	return 0;
}

int main(int argc, char **argv)
try {
	const auto c = ParseCommandLine(argc, argv);

	/* initialize MPD */

	const GlobalInit init(c.config_path, c.verbose);

	/* open the stream and dump it */

	Mutex mutex;
	Cond cond;
	auto is = InputStream::OpenReady(c.uri, mutex, cond);
	return dump_input_stream(is.get());
} catch (const std::exception &e) {
	LogError(e);
	return EXIT_FAILURE;
}
