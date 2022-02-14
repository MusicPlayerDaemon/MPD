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
#include "ConfigGlue.hxx"
#include "event/Thread.hxx"
#include "input/Init.hxx"
#include "input/InputStream.hxx"
#include "input/TextInputStream.hxx"
#include "util/PrintException.hxx"

#ifdef ENABLE_ARCHIVE
#include "archive/ArchiveList.hxx"
#endif

#include <stdexcept>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

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
dump_text_file(TextInputStream &is)
{
	const char *line;
	while ((line = is.ReadLine()) != nullptr)
		printf("'%s'\n", line);
}

static int
dump_input_stream(InputStreamPtr &&is)
{
	{
		TextInputStream tis(std::move(is));
		dump_text_file(tis);
	}

	const std::scoped_lock<Mutex> protect(is->mutex);

	is->Check();
	return 0;
}

int main(int argc, char **argv)
try {
	if (argc != 2) {
		fprintf(stderr, "Usage: dump_text_file URI\n");
		return EXIT_FAILURE;
	}

	/* initialize MPD */

	const GlobalInit init{nullptr};

	/* open the stream and dump it */

	Mutex mutex;

	auto is = InputStream::OpenReady(argv[1], mutex);
	return dump_input_stream(std::move(is));
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
