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
#include "conf.h"
#include "audio_parser.h"
#include "audio_format.h"
#include "filter_plugin.h"
#include "pcm_volume.h"
#include "idle.h"
#include "mixer_control.h"
#include "playlist.h"
#include "stdbin.h"

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

struct playlist g_playlist;

void
idle_add(G_GNUC_UNUSED unsigned flags)
{
}

bool
mixer_set_volume(G_GNUC_UNUSED struct mixer *mixer,
		 G_GNUC_UNUSED unsigned volume, G_GNUC_UNUSED GError **error_r)
{
	return true;
}

static void
my_log_func(const gchar *log_domain, G_GNUC_UNUSED GLogLevelFlags log_level,
	    const gchar *message, G_GNUC_UNUSED gpointer user_data)
{
	if (log_domain != NULL)
		g_printerr("%s: %s\n", log_domain, message);
	else
		g_printerr("%s\n", message);
}

static const struct config_param *
find_named_config_block(const char *block, const char *name)
{
	const struct config_param *param = NULL;

	while ((param = config_get_next_param(block, param)) != NULL) {
		const char *current_name =
			config_get_block_string(param, "name", NULL);
		if (current_name != NULL && strcmp(current_name, name) == 0)
			return param;
	}

	return NULL;
}

static struct filter *
load_filter(const char *name)
{
	const struct config_param *param;
	struct filter *filter;
	GError *error = NULL;

	param = find_named_config_block("filter", name);
	if (param == NULL) {
		g_printerr("No such configured filter: %s\n", name);
		return false;
	}

	filter = filter_configured_new(param, &error);
	if (filter == NULL) {
		g_printerr("Failed to load filter: %s\n", error->message);
		g_error_free(error);
		return NULL;
	}

	return filter;
}

int main(int argc, char **argv)
{
	struct audio_format audio_format;
	struct audio_format_string af_string;
	bool success;
	GError *error = NULL;
	struct filter *filter;
	const struct audio_format *out_audio_format;
	char buffer[4096];

	if (argc < 3 || argc > 4) {
		g_printerr("Usage: run_filter CONFIG NAME [FORMAT] <IN\n");
		return 1;
	}

	audio_format_init(&audio_format, 44100, SAMPLE_FORMAT_S16, 2);

	/* initialize GLib */

	g_thread_init(NULL);
	g_log_set_default_handler(my_log_func, NULL);

	/* read configuration file (mpd.conf) */

	config_global_init();
	success = config_read_file(argv[1], &error);
	if (!success) {
		g_printerr("%s:", error->message);
		g_error_free(error);
		return 1;
	}

	/* parse the audio format */

	if (argc > 3) {
		success = audio_format_parse(&audio_format, argv[3],
					     false, &error);
		if (!success) {
			g_printerr("Failed to parse audio format: %s\n",
				   error->message);
			g_error_free(error);
			return 1;
		}
	}

	/* initialize the filter */

	filter = load_filter(argv[2]);
	if (filter == NULL)
		return 1;

	/* open the filter */

	out_audio_format = filter_open(filter, &audio_format, &error);
	if (out_audio_format == NULL) {
		g_printerr("Failed to open filter: %s\n", error->message);
		g_error_free(error);
		filter_free(filter);
		return 1;
	}

	g_printerr("audio_format=%s\n",
		   audio_format_to_string(out_audio_format, &af_string));

	/* play */

	while (true) {
		ssize_t nbytes;
		size_t length;
		const void *dest;

		nbytes = read(0, buffer, sizeof(buffer));
		if (nbytes <= 0)
			break;

		dest = filter_filter(filter, buffer, (size_t)nbytes,
				     &length, &error);
		if (dest == NULL) {
			g_printerr("Filter failed: %s\n", error->message);
			filter_close(filter);
			filter_free(filter);
			return 1;
		}

		nbytes = write(1, dest, length);
		if (nbytes < 0) {
			g_printerr("Failed to write: %s\n", g_strerror(errno));
			filter_close(filter);
			filter_free(filter);
			return 1;
		}
	}

	/* cleanup and exit */

	filter_close(filter);
	filter_free(filter);

	config_global_finish();

	return 0;
}
