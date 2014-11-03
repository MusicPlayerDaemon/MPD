/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "ScopeIOThread.hxx"
#include "input/Init.hxx"
#include "input/InputStream.hxx"
#include "input/TextInputStream.hxx"
#include "config/ConfigGlobal.hxx"
#include "stdbin.h"
#include "util/Error.hxx"
#include "thread/Cond.hxx"
#include "Log.hxx"

#ifdef ENABLE_ARCHIVE
#include "archive/ArchiveList.hxx"
#endif

#ifdef HAVE_GLIB
#include <glib.h>
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

static void
dump_text_file(TextInputStream &is)
{
	const char *line;
	while ((line = is.ReadLine()) != nullptr)
		printf("'%s'\n", line);
}

static int
dump_input_stream(InputStream &is)
{
	{
		TextInputStream tis(is);
		dump_text_file(tis);
	}

	is.Lock();

	Error error;
	if (!is.Check(error)) {
		LogError(error);
		is.Unlock();
		return EXIT_FAILURE;
	}

	is.Unlock();

	return 0;
}

int main(int argc, char **argv)
{
	int ret;

	if (argc != 2) {
		fprintf(stderr, "Usage: run_input URI\n");
		return EXIT_FAILURE;
	}

	/* initialize GLib */

#ifdef HAVE_GLIB
#if !GLIB_CHECK_VERSION(2,32,0)
	g_thread_init(NULL);
#endif
#endif

	/* initialize MPD */

	config_global_init();

	const ScopeIOThread io_thread;

#ifdef ENABLE_ARCHIVE
	archive_plugin_init_all();
#endif

	Error error;
	if (!input_stream_global_init(error)) {
		LogError(error);
		return 2;
	}

	/* open the stream and dump it */

	Mutex mutex;
	Cond cond;

	InputStream *is = InputStream::OpenReady(argv[1], mutex, cond, error);
	if (is != NULL) {
		ret = dump_input_stream(*is);
		delete is;
	} else {
		if (error.IsDefined())
			LogError(error);
		else
			fprintf(stderr, "input_stream::Open() failed\n");
		ret = EXIT_FAILURE;
	}

	/* deinitialize everything */

	input_stream_global_finish();

#ifdef ENABLE_ARCHIVE
	archive_plugin_deinit_all();
#endif

	config_global_finish();

	return ret;
}
