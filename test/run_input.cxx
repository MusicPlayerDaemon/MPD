/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
#include "fs/NarrowPath.hxx"
#include "io/BufferedOutputStream.hxx"
#include "io/StdioOutputStream.hxx"
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

static constexpr std::size_t MAX_CHUNK_SIZE = 16384;

struct CommandLine {
	const char *uri = nullptr;

	FromNarrowPath config_path;

	std::size_t seek = 0;

	std::size_t chunk_size = MAX_CHUNK_SIZE;

	bool verbose = false;

	bool scan = false;
};

enum Option {
	OPTION_CONFIG,
	OPTION_VERBOSE,
	OPTION_SCAN,
	OPTION_SEEK,
	OPTION_CHUNK_SIZE,
};

static constexpr OptionDef option_defs[] = {
	{"config", 0, true, "Load a MPD configuration file"},
	{"verbose", 'v', false, "Verbose logging"},
	{"scan", 0, false, "Scan tags instead of reading raw data"},
	{"seek", 0, true, "Start reading at this position"},
	{"chunk-size", 0, true, "Read this number of bytes at a time"},
};

static std::size_t
ParseSize(const char *s)
{
	char *endptr;
	std::size_t value = std::strtoul(s, &endptr, 10);
	if (endptr == s)
		throw std::runtime_error("Failed to parse integer");

	return value;
}

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

		case OPTION_SCAN:
			c.scan = true;
			break;

		case OPTION_SEEK:
			c.seek = ParseSize(o.value);
			break;

		case OPTION_CHUNK_SIZE:
			c.chunk_size = ParseSize(o.value);
			if (c.chunk_size <= 0 || c.chunk_size > MAX_CHUNK_SIZE)
				throw std::runtime_error("Invalid chunk size");
			break;
		}
	}

	auto args = option_parser.GetRemaining();
	if (args.size != 1)
		throw std::runtime_error("Usage: run_input [--verbose] [--config=FILE] [--scan] [--chunk-size=BYTES] URI");

	c.uri = args.front();
	return c;
}

class GlobalInit {
	const ConfigData config;
	EventThread io_thread;

#ifdef ENABLE_ARCHIVE
	const ScopeArchivePluginsInit archive_plugins_init{config};
#endif

	const ScopeInputPluginsInit input_plugins_init{config, io_thread.GetEventLoop()};

public:
	explicit GlobalInit(Path config_path)
		:config(AutoLoadConfigFile(config_path))
	{
		io_thread.Start();
	}
};

static void
tag_save(FILE *file, const Tag &tag)
{
	StdioOutputStream sos(file);
	WithBufferedOutputStream(sos, [&](auto &bos){
		tag_save(bos, tag);
	});
}

static int
dump_input_stream(InputStream &is, FileDescriptor out,
		  offset_type seek, size_t chunk_size)
{
	out.SetBinaryMode();

	std::unique_lock<Mutex> lock(is.mutex);

	if (seek > 0)
		is.Seek(lock, seek);

	/* print meta data */

	if (is.HasMimeType())
		fprintf(stderr, "MIME type: %s\n", is.GetMimeType());

	/* read data and tags from the stream */

	while (!is.IsEOF()) {
		{
			auto tag = is.ReadTag();
			if (tag) {
				fprintf(stderr, "Received a tag:\n");
				tag_save(stderr, *tag);
			}
		}

		char buffer[MAX_CHUNK_SIZE];
		assert(chunk_size <= sizeof(buffer));
		size_t num_read = is.Read(lock, buffer, chunk_size);
		if (num_read == 0)
			break;

		out.FullWrite(buffer, num_read);
	}

	is.Check();

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
		const std::scoped_lock<Mutex> lock(mutex);
		tag = std::move(_tag);
		done = true;
		cond.notify_all();
	}

	void OnRemoteTagError(std::exception_ptr e) noexcept override {
		const std::scoped_lock<Mutex> lock(mutex);
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
	return dump_input_stream(*is, FileDescriptor(STDOUT_FILENO),
				 c.seek, c.chunk_size);
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
