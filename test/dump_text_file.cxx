/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "IOThread.hxx"
#include "InputInit.hxx"
#include "InputStream.hxx"
#include "conf.h"
#include "stdbin.h"
#include "TextInputStream.hxx"
#include "util/Error.hxx"

#ifdef ENABLE_ARCHIVE
#include "ArchiveList.hxx"
#endif

#include <glib.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

static void
my_log_func(const gchar *log_domain, gcc_unused GLogLevelFlags log_level,
	    const gchar *message, gcc_unused gpointer user_data)
{
	if (log_domain != NULL)
		g_printerr("%s: %s\n", log_domain, message);
	else
		g_printerr("%s\n", message);
}

static void
dump_text_file(TextInputStream &is)
{
	std::string line;
	while (is.ReadLine(line))
		printf("'%s'\n", line.c_str());
}

static int
dump_input_stream(struct input_stream *is)
{
	Error error;

	is->Lock();

	/* wait until the stream becomes ready */

	is->WaitReady();

	if (!is->Check(error)) {
		g_warning("%s", error.GetMessage());
		is->Unlock();
		return EXIT_FAILURE;
	}

	/* read data and tags from the stream */

	is->Unlock();
	{
		TextInputStream tis(is);
		dump_text_file(tis);
	}

	is->Lock();

	if (!is->Check(error)) {
		g_warning("%s", error.GetMessage());
		is->Unlock();
		return EXIT_FAILURE;
	}

	is->Unlock();

	return 0;
}

int main(int argc, char **argv)
{
	struct input_stream *is;
	int ret;

	if (argc != 2) {
		g_printerr("Usage: run_input URI\n");
		return 1;
	}

	/* initialize GLib */

#if !GLIB_CHECK_VERSION(2,32,0)
	g_thread_init(NULL);
#endif

	g_log_set_default_handler(my_log_func, NULL);

	/* initialize MPD */

	config_global_init();

	io_thread_init();
	io_thread_start();

#ifdef ENABLE_ARCHIVE
	archive_plugin_init_all();
#endif

	Error error;
	if (!input_stream_global_init(error)) {
		g_warning("%s", error.GetMessage());
		return 2;
	}

	/* open the stream and dump it */

	Mutex mutex;
	Cond cond;

	is = input_stream::Open(argv[1], mutex, cond, error);
	if (is != NULL) {
		ret = dump_input_stream(is);
		is->Close();
	} else {
		if (error.IsDefined())
			g_warning("%s", error.GetMessage());
		else
			g_printerr("input_stream::Open() failed\n");
		ret = 2;
	}

	/* deinitialize everything */

	input_stream_global_finish();

#ifdef ENABLE_ARCHIVE
	archive_plugin_deinit_all();
#endif

	io_thread_deinit();

	config_global_finish();

	return ret;
}
