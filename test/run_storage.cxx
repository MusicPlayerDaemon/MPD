// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "cmdline/OptionDef.hxx"
#include "cmdline/OptionParser.hxx"
#include "event/Thread.hxx"
#include "storage/Registry.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "net/Init.hxx"
#include "time/ChronoUtil.hxx"
#include "time/ISO8601.hxx"
#include "util/PrintException.hxx"
#include "util/StringAPI.hxx"
#include "util/StringBuffer.hxx"
#include "Log.hxx"
#include "LogBackend.hxx"

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
)";

struct CommandLine {
	bool verbose = false;

	const char *command;

	std::span<const char *const> args;
};

enum class Option {
	VERBOSE,
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
		switch (static_cast<Option>(o.index)) {
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
	const ScopeNetInit net_init;
	EventThread io_thread;

public:
	GlobalInit() {
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

int
main(int argc, char **argv)
try {
	const auto c = ParseCommandLine(argc, argv);

	SetLogThreshold(c.verbose ? LogLevel::DEBUG : LogLevel::INFO);
	GlobalInit init;

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
	} else {
		fprintf(stderr, "Unknown command\n\n%s", usage_text);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
