/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "input_stream.h"
#include "tag_pool.h"
#include "tag_save.h"
#include "conf.h"

#include <glib.h>

#include <unistd.h>

static void
my_log_func(const gchar *log_domain, G_GNUC_UNUSED GLogLevelFlags log_level,
	    const gchar *message, G_GNUC_UNUSED gpointer user_data)
{
	if (log_domain != NULL)
		g_printerr("%s: %s\n", log_domain, message);
	else
		g_printerr("%s\n", message);
}

int main(int argc, char **argv)
{
	struct input_stream is;
	bool success;
	char buffer[4096];
	size_t num_read;
	ssize_t num_written;

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
	input_stream_global_init();

	/* open the stream and wait until it becomes ready */

	success = input_stream_open(&is, argv[1]);
	if (!success) {
		g_printerr("input_stream_open() failed\n");
		return 2;
	}

	while (!is.ready) {
		int ret = input_stream_buffer(&is);
		if (ret < 0)
			/* error */
			return 2;

		if (ret == 0)
			/* nothing was buffered - wait */
			g_usleep(10000);
	}

	/* print meta data */

	if (is.mime != NULL)
		g_printerr("MIME type: %s\n", is.mime);

	/* read data and tags from the stream */

	while (!input_stream_eof(&is)) {
		struct tag *tag = input_stream_tag(&is);
		if (tag != NULL) {
			g_printerr("Received a tag:\n");
			tag_save(stderr, tag);
			tag_free(tag);
		}

		num_read = input_stream_read(&is, buffer, sizeof(buffer));
		if (num_read == 0)
			break;

		num_written = write(1, buffer, num_read);
		if (num_written <= 0)
			break;
	}

	/* deinitialize everything */

	input_stream_close(&is);
	input_stream_global_finish();
	config_global_finish();
	tag_pool_deinit();

	return 0;
}
