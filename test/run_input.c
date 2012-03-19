/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "io_thread.h"
#include "input_init.h"
#include "input_stream.h"
#include "tag_pool.h"
#include "tag_save.h"
#include "conf.h"
#include "stdbin.h"

#ifdef ENABLE_ARCHIVE
#include "archive_list.h"
#endif

#include <glib.h>

#include <unistd.h>
#include <stdlib.h>

static void
my_log_func(const gchar *log_domain, G_GNUC_UNUSED GLogLevelFlags log_level,
	    const gchar *message, G_GNUC_UNUSED gpointer user_data)
{
	if (log_domain != NULL)
		g_printerr("%s: %s\n", log_domain, message);
	else
		g_printerr("%s\n", message);
}

static int
dump_input_stream(struct input_stream *is)
{
	GError *error = NULL;
	char buffer[4096];
	size_t num_read;
	ssize_t num_written;

	input_stream_lock(is);

	/* wait until the stream becomes ready */

	input_stream_wait_ready(is);

	if (!input_stream_check(is, &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		input_stream_unlock(is);
		return EXIT_FAILURE;
	}

	/* print meta data */

	if (is->mime != NULL)
		g_printerr("MIME type: %s\n", is->mime);

	/* read data and tags from the stream */

	while (!input_stream_eof(is)) {
		struct tag *tag = input_stream_tag(is);
		if (tag != NULL) {
			g_printerr("Received a tag:\n");
			tag_save(stderr, tag);
			tag_free(tag);
		}

		num_read = input_stream_read(is, buffer, sizeof(buffer),
					     &error);
		if (num_read == 0) {
			if (error != NULL) {
				g_warning("%s", error->message);
				g_error_free(error);
			}

			break;
		}

		num_written = write(1, buffer, num_read);
		if (num_written <= 0)
			break;
	}

	if (!input_stream_check(is, &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		input_stream_unlock(is);
		return EXIT_FAILURE;
	}

	input_stream_unlock(is);

	return 0;
}

int main(int argc, char **argv)
{
	GError *error = NULL;
	struct input_stream *is;
	int ret;

	if (argc != 2) {
		g_printerr("Usage: run_input URI\n");
		return 1;
	}

	/* initialize GLib */

	g_thread_init(NULL);
	g_log_set_default_handler(my_log_func, NULL);

	/* initialize MPD */

	tag_pool_init();
	config_global_init();

	io_thread_init();
	if (!io_thread_start(&error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return EXIT_FAILURE;
	}

#ifdef ENABLE_ARCHIVE
	archive_plugin_init_all();
#endif

	if (!input_stream_global_init(&error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return 2;
	}

	/* open the stream and dump it */

	GMutex *mutex = g_mutex_new();
	GCond *cond = g_cond_new();

	is = input_stream_open(argv[1], mutex, cond, &error);
	if (is != NULL) {
		ret = dump_input_stream(is);
		input_stream_close(is);
	} else {
		if (error != NULL) {
			g_warning("%s", error->message);
			g_error_free(error);
		} else
			g_printerr("input_stream_open() failed\n");
		ret = 2;
	}

	g_cond_free(cond);
	g_mutex_free(mutex);

	/* deinitialize everything */

	input_stream_global_finish();

#ifdef ENABLE_ARCHIVE
	archive_plugin_deinit_all();
#endif

	io_thread_deinit();

	config_global_finish();
	tag_pool_deinit();

	return ret;
}
