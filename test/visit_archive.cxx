// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "tag/Tag.hxx"
#include "ConfigGlue.hxx"
#include "event/Thread.hxx"
#include "input/Init.hxx"
#include "archive/ArchiveList.hxx"
#include "archive/ArchivePlugin.hxx"
#include "archive/ArchiveFile.hxx"
#include "archive/ArchiveVisitor.hxx"
#include "fs/NarrowPath.hxx"
#include "fs/Path.hxx"
#include "util/PrintException.hxx"

#include <stdexcept>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

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

class MyArchiveVisitor final : public ArchiveVisitor {
public:
	void VisitArchiveEntry(const char *path_utf8) override {
		printf("%s\n", path_utf8);
	}
};

int
main(int argc, char **argv)
try {
	if (argc != 3) {
		fprintf(stderr, "Usage: visit_archive PLUGIN PATH\n");
		return EXIT_FAILURE;
	}

	const char *plugin_name = argv[1];
	const Path path = FromNarrowPath(argv[2]);

	/* initialize MPD */

	const GlobalInit init{nullptr};

	/* open the archive and dump it */

	const ArchivePlugin *plugin = archive_plugin_from_name(plugin_name);
	if (plugin == nullptr) {
		fprintf(stderr, "No such plugin: %s\n", plugin_name);
		return EXIT_FAILURE;
	}

	int result = EXIT_SUCCESS;

	auto file = archive_file_open(plugin, path);

	MyArchiveVisitor visitor;
	file->Visit(visitor);

	return result;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
