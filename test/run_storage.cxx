// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "cmdline/OptionDef.hxx"
#include "cmdline/OptionParser.hxx"
#include "event/Thread.hxx"
#include "ConfigGlue.hxx"
#include "tag/Tag.hxx"
#include "storage/Registry.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "input/Init.hxx"
#include "input/InputStream.hxx"
#include "input/CondHandler.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "event/Thread.hxx"
#include "net/Init.hxx"
#include "io/BufferedOutputStream.hxx"
#include "io/FileDescriptor.hxx"
#include "io/StdioOutputStream.hxx"
#include "time/ChronoUtil.hxx"
#include "time/ISO8601.hxx"
#include "util/PrintException.hxx"
#include "util/StringAPI.hxx"
#include "util/StringBuffer.hxx"
#include "Log.hxx"
#include "LogBackend.hxx"
#include "TagSave.hxx"
#include "config.h"

#ifdef ENABLE_ARCHIVE
#include "archive/ArchiveList.hxx"
#endif

#include <memory>
#include <stdexcept>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static constexpr auto usage_text = R"(Usage: run_storage [OPTIONS] COMMAND URI ...

Options:
  --verbose

Available commands:
  ls URI PATH
  stat URI PATH
  cat URI PATH
)";

struct CommandLine {
	FromNarrowPath config_path;

	bool verbose = false;

	const char *command;

	std::span<const char *const> args;
};

enum class Option {
	CONFIG,
	VERBOSE,
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
		switch (static_cast<Option>(o.index)) {
		case Option::CONFIG:
			c.config_path = o.value;
			break;

		case Option::VERBOSE:
			c.verbose = true;
			break;
		}
	}

	auto args = option_parser.GetRemaining();
	if (args.empty())
		throw std::runtime_error{usage_text};

	c.command = args.front();
	c.args = args.subspan(1);
	return c;
}

class GlobalInit {
	const ConfigData config;
	const ScopeNetInit net_init;
	EventThread io_thread;

#ifdef ENABLE_ARCHIVE
	const ScopeArchivePluginsInit archive_plugins_init{config};
#endif

	const ScopeInputPluginsInit input_plugins_init{config, io_thread.GetEventLoop()};

public:
	GlobalInit(Path config_path)
		:config(AutoLoadConfigFile(config_path))
	{
		io_thread.Start();
	}

	EventLoop &GetEventLoop() noexcept {
		return io_thread.GetEventLoop();
	}
};

static std::unique_ptr<Storage>
MakeStorage(EventLoop &event_loop, const char *uri)
{
	auto storage = CreateStorageURI(event_loop, uri);
	if (storage == nullptr)
		throw std::runtime_error("Unrecognized storage URI");

	return storage;
}

static int
Ls(Storage &storage, const char *path)
{
	auto dir = storage.OpenDirectory(path);

	const char *name;
	while ((name = dir->Read()) != nullptr) {
		const auto info = dir->GetInfo(false);

		const char *type = "unk";
		switch (info.type) {
		case StorageFileInfo::Type::OTHER:
			type = "oth";
			break;

		case StorageFileInfo::Type::REGULAR:
			type = "reg";
			break;

		case StorageFileInfo::Type::DIRECTORY:
			type = "dir";
			break;
		}

		StringBuffer<64> mtime_buffer;
		const char *mtime = "          ";
		if (!IsNegative(info.mtime)) {
			mtime_buffer = FormatISO8601(info.mtime);
			mtime = mtime_buffer;
		}

		printf("%s %10llu %s %s\n",
		       type, (unsigned long long)info.size,
		       mtime, name);
	}

	return EXIT_SUCCESS;
}

static int
Stat(Storage &storage, const char *path)
{
	const auto info = storage.GetInfo(path, false);
	switch (info.type) {
	case StorageFileInfo::Type::OTHER:
		printf("other\n");
		break;

	case StorageFileInfo::Type::REGULAR:
		printf("regular\n");
		break;

	case StorageFileInfo::Type::DIRECTORY:
		printf("directory\n");
		break;
	}

	printf("size: %llu\n", (unsigned long long)info.size);

	return EXIT_SUCCESS;
}

static void
tag_save(FILE *file, const Tag &tag)
{
	StdioOutputStream sos(file);
	WithBufferedOutputStream(sos, [&](auto &bos){
		tag_save(bos, tag);
	});
}

static void
WaitReady(InputStream &is, std::unique_lock<Mutex> &lock)
{
	CondInputStreamHandler handler;
	is.SetHandler(&handler);

	handler.cond.wait(lock, [&is]{
		is.Update();
		return is.IsReady();
	});

	is.Check();
}

static void
Cat(InputStream &is, std::unique_lock<Mutex> &lock, FileDescriptor out)
{
	assert(is.IsReady());

	out.SetBinaryMode();

	if (is.HasMimeType())
		fprintf(stderr, "MIME type: %s\n", is.GetMimeType());

	/* read data and tags from the stream */

	while (!is.IsEOF()) {
		if (const auto tag = is.ReadTag()) {
			fprintf(stderr, "Received a tag:\n");
			tag_save(stderr, *tag);
		}

		std::byte buffer[16384];
		const auto nbytes = is.Read(lock, buffer);
		if (nbytes == 0)
			break;

		out.FullWrite({buffer, nbytes});
	}

	is.Check();
}

static int
Cat(Storage &storage, const char *path)
{
	Mutex mutex;
	auto is = storage.OpenFile(path, mutex);
	assert(is);

	std::unique_lock lock{mutex};
	WaitReady(*is, lock);
	Cat(*is, lock, FileDescriptor{STDOUT_FILENO});

	return EXIT_SUCCESS;
}

int
main(int argc, char **argv)
try {
	const auto c = ParseCommandLine(argc, argv);

	SetLogThreshold(c.verbose ? LogLevel::DEBUG : LogLevel::INFO);
	GlobalInit init{c.config_path};

	if (StringIsEqual(c.command, "ls")) {
		if (c.args.size() != 2) {
			fputs(usage_text, stderr);
			return EXIT_FAILURE;
		}

		const char *const storage_uri = c.args[0];
		const char *const path = c.args[1];

		auto storage = MakeStorage(init.GetEventLoop(),
					   storage_uri);

		return Ls(*storage, path);
	} else if (StringIsEqual(c.command, "stat")) {
		if (c.args.size() != 2) {
			fputs(usage_text, stderr);
			return EXIT_FAILURE;
		}

		const char *const storage_uri = c.args[0];
		const char *const path = c.args[1];

		auto storage = MakeStorage(init.GetEventLoop(),
					   storage_uri);

		return Stat(*storage, path);
	} else if (StringIsEqual(c.command, "cat")) {
		if (c.args.size() != 2) {
			fputs(usage_text, stderr);
			return EXIT_FAILURE;
		}

		const char *const storage_uri = c.args[0];
		const char *const path = c.args[1];

		auto storage = MakeStorage(init.GetEventLoop(),
					   storage_uri);

		return Cat(*storage, path);
	} else {
		fprintf(stderr, "Unknown command\n\n%s", usage_text);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
