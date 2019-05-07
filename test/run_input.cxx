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

#include "config.h"
#include "TagSave.hxx"
#include "tag/Tag.hxx"
#include "ConfigGlue.hxx"
#include "input/InputStream.hxx"
#include "input/Init.hxx"
#include "input/Registry.hxx"
#include "input/InputPlugin.hxx"
#include "input/RemoteTagScanner.hxx"
#include "input/ScanTags.hxx"
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
#include "util/PrintException.hxx"

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

	bool scan = false;
};

enum Option {
	OPTION_CONFIG,
	OPTION_VERBOSE,
	OPTION_SCAN,
};

static constexpr OptionDef option_defs[] = {
	{"config", 0, true, "Load a MPD configuration file"},
	{"verbose", 'v', false, "Verbose logging"},
	{"scan", 0, false, "Scan tags instead of reading raw data"},
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

		case OPTION_SCAN:
			c.scan = true;
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
	const ConfigData config;
	EventThread io_thread;

#ifdef ENABLE_ARCHIVE
	const ScopeArchivePluginsInit archive_plugins_init;
#endif

	const ScopeInputPluginsInit input_plugins_init;

public:
	explicit GlobalInit(Path config_path)
		:config(AutoLoadConfigFile(config_path)),
		 input_plugins_init(config, io_thread.GetEventLoop())
	{
		io_thread.Start();
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
	std::unique_lock<Mutex> lock(is->mutex);

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
		size_t num_read = is->Read(lock, buffer, sizeof(buffer));
		if (num_read == 0)
			break;

		ssize_t num_written = write(1, buffer, num_read);
		if (num_written <= 0)
			break;
	}

	is->Check();

	return 0;
}

class DumpRemoteTagHandler final : public RemoteTagHandler {
	Mutex mutex;
	Cond cond;

	Tag tag;
	std::exception_ptr error;

	bool done = false;

public:
	Tag Wait() {
		std::unique_lock<Mutex> lock(mutex);
		cond.wait(lock, [this]{ return done; });

		if (error)
			std::rethrow_exception(error);

		return std::move(tag);
	}

	/* virtual methods from RemoteTagHandler */
	void OnRemoteTag(Tag &&_tag) noexcept override {
		const std::lock_guard<Mutex> lock(mutex);
		tag = std::move(_tag);
		done = true;
		cond.notify_all();
	}

	void OnRemoteTagError(std::exception_ptr e) noexcept override {
		const std::lock_guard<Mutex> lock(mutex);
		error = std::move(e);
		done = true;
		cond.notify_all();
	}
};

static int
Scan(const char *uri)
{
	DumpRemoteTagHandler handler;

	auto scanner = InputScanTags(uri, handler);
	if (!scanner) {
		fprintf(stderr, "Unsupported URI\n");
		return EXIT_FAILURE;
	}

	scanner->Start();
	tag_save(stdout, handler.Wait());
	return EXIT_SUCCESS;
}

int main(int argc, char **argv)
try {
	const auto c = ParseCommandLine(argc, argv);

	/* initialize MPD */

	SetLogThreshold(c.verbose ? LogLevel::DEBUG : LogLevel::INFO);
	const GlobalInit init(c.config_path);

	if (c.scan)
		return Scan(c.uri);

	/* open the stream and dump it */

	Mutex mutex;
	auto is = InputStream::OpenReady(c.uri, mutex);
	return dump_input_stream(is.get());
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
