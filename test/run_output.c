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
#include "output_plugin.h"
#include "output_internal.h"
#include "output_control.h"
#include "conf.h"
#include "audio_parser.h"
#include "filter_registry.h"
#include "pcm_convert.h"
#include "event_pipe.h"
#include "idle.h"
#include "playlist.h"
#include "player_control.h"
#include "stdbin.h"

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

struct playlist g_playlist;

void
idle_add(G_GNUC_UNUSED unsigned flags)
{
}

void
event_pipe_emit(G_GNUC_UNUSED enum pipe_event event)
{
}

void pcm_convert_init(G_GNUC_UNUSED struct pcm_convert_state *state)
{
}

void pcm_convert_deinit(G_GNUC_UNUSED struct pcm_convert_state *state)
{
}

const void *
pcm_convert(G_GNUC_UNUSED struct pcm_convert_state *state,
	    G_GNUC_UNUSED const struct audio_format *src_format,
	    G_GNUC_UNUSED const void *src, G_GNUC_UNUSED size_t src_size,
	    G_GNUC_UNUSED const struct audio_format *dest_format,
	    G_GNUC_UNUSED size_t *dest_size_r,
	    GError **error_r)
{
	g_set_error(error_r, pcm_convert_quark(), 0,
		    "Not implemented");
	return NULL;
}

const struct filter_plugin *
filter_plugin_by_name(G_GNUC_UNUSED const char *name)
{
	assert(false);
	return NULL;
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

static struct audio_output *
load_audio_output(const char *name)
{
	const struct config_param *param;
	GError *error = NULL;

	param = find_named_config_block(CONF_AUDIO_OUTPUT, name);
	if (param == NULL) {
		g_printerr("No such configured audio output: %s\n", name);
		return false;
	}

	static struct player_control dummy_player_control;

	struct audio_output *ao =
		audio_output_new(param, &dummy_player_control, &error);
	if (ao == NULL) {
		g_printerr("%s\n", error->message);
		g_error_free(error);
	}

	return ao;
}

static bool
run_output(struct audio_output *ao, struct audio_format *audio_format)
{
	/* open the audio output */

	GError *error = NULL;
	if (!ao_plugin_enable(ao, &error)) {
		g_printerr("Failed to enable audio output: %s\n",
			   error->message);
		g_error_free(error);
		return false;
	}

	if (!ao_plugin_open(ao, audio_format, &error)) {
		ao_plugin_disable(ao);
		g_printerr("Failed to open audio output: %s\n",
			   error->message);
		g_error_free(error);
		return false;
	}

	struct audio_format_string af_string;
	g_printerr("audio_format=%s\n",
		   audio_format_to_string(audio_format, &af_string));

	size_t frame_size = audio_format_frame_size(audio_format);

	/* play */

	size_t length = 0;
	char buffer[4096];
	while (true) {
		if (length < sizeof(buffer)) {
			ssize_t nbytes = read(0, buffer + length,
					      sizeof(buffer) - length);
			if (nbytes <= 0)
				break;

			length += (size_t)nbytes;
		}

		size_t play_length = (length / frame_size) * frame_size;
		if (play_length > 0) {
			size_t consumed = ao_plugin_play(ao,
							 buffer, play_length,
							 &error);
			if (consumed == 0) {
				ao_plugin_close(ao);
				ao_plugin_disable(ao);
				g_printerr("Failed to play: %s\n",
					   error->message);
				g_error_free(error);
				return false;
			}

			assert(consumed <= length);
			assert(consumed % frame_size == 0);

			length -= consumed;
			memmove(buffer, buffer + consumed, length);
		}
	}

	ao_plugin_close(ao);
	ao_plugin_disable(ao);
	return true;
}

int main(int argc, char **argv)
{
	struct audio_format audio_format;
	bool success;
	GError *error = NULL;

	if (argc < 3 || argc > 4) {
		g_printerr("Usage: run_output CONFIG NAME [FORMAT] <IN\n");
		return 1;
	}

	audio_format_init(&audio_format, 44100, SAMPLE_FORMAT_S16, 2);

	g_thread_init(NULL);

	/* read configuration file (mpd.conf) */

	config_global_init();
	success = config_read_file(argv[1], &error);
	if (!success) {
		g_printerr("%s:", error->message);
		g_error_free(error);
		return 1;
	}

	io_thread_init();
	if (!io_thread_start(&error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return EXIT_FAILURE;
	}

	/* initialize the audio output */

	struct audio_output *ao = load_audio_output(argv[2]);
	if (ao == NULL)
		return 1;

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

	/* do it */

	success = run_output(ao, &audio_format);

	/* cleanup and exit */

	audio_output_free(ao);

	io_thread_deinit();

	config_global_finish();

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
