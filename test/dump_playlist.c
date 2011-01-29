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
#include "input_init.h"
#include "input_stream.h"
#include "tag_pool.h"
#include "tag_save.h"
#include "conf.h"
#include "song.h"
#include "playlist_list.h"
#include "playlist_plugin.h"

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
	const char *uri;
	struct input_stream *is = NULL;
	bool success;
	GError *error = NULL;
	struct playlist_provider *playlist;
	struct song *song;

	if (argc != 3) {
		g_printerr("Usage: dump_playlist CONFIG URI\n");
		return 1;
	}

	uri = argv[2];

	/* initialize GLib */

	g_thread_init(NULL);
	g_log_set_default_handler(my_log_func, NULL);

	/* initialize MPD */

	tag_pool_init();
	config_global_init();
	success = config_read_file(argv[1], &error);
	if (!success) {
		g_printerr("%s:", error->message);
		g_error_free(error);
		return 1;
	}

	if (!input_stream_global_init(&error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return 2;
	}

	playlist_list_global_init();

	/* open the playlist */

	playlist = playlist_list_open_uri(uri);
	if (playlist == NULL) {
		/* open the stream and wait until it becomes ready */

		is = input_stream_open(uri, &error);
		if (is == NULL) {
			if (error != NULL) {
				g_warning("%s", error->message);
				g_error_free(error);
			} else
				g_printerr("input_stream_open() failed\n");
			return 2;
		}

		while (!is->ready) {
			int ret = input_stream_buffer(is, &error);
			if (ret < 0) {
				/* error */
				g_warning("%s", error->message);
				g_error_free(error);
				return 2;
			}

			if (ret == 0)
				/* nothing was buffered - wait */
				g_usleep(10000);
		}

		/* open the playlist */

		playlist = playlist_list_open_stream(is, uri);
		if (playlist == NULL) {
			input_stream_close(is);
			g_printerr("Failed to open playlist\n");
			return 2;
		}
	}

	/* dump the playlist */

	while ((song = playlist_plugin_read(playlist)) != NULL) {
		g_print("%s\n", song->uri);

		if (song->end_ms > 0)
			g_print("range: %u:%02u..%u:%02u\n",
				song->start_ms / 60000,
				(song->start_ms / 1000) % 60,
				song->end_ms / 60000,
				(song->end_ms / 1000) % 60);
		else if (song->start_ms > 0)
			g_print("range: %u:%02u..\n",
				song->start_ms / 60000,
				(song->start_ms / 1000) % 60);

		if (song->tag != NULL)
			tag_save(stdout, song->tag);

		song_free(song);
	}

	/* deinitialize everything */

	playlist_plugin_close(playlist);
	if (is != NULL)
		input_stream_close(is);
	playlist_list_global_finish();
	input_stream_global_finish();
	config_global_finish();
	tag_pool_deinit();

	return 0;
}
