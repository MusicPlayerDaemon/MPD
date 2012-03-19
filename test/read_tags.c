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
#include "decoder_list.h"
#include "decoder_api.h"
#include "input_init.h"
#include "input_stream.h"
#include "audio_format.h"
#include "pcm_volume.h"
#include "tag_ape.h"
#include "tag_id3.h"
#include "tag_handler.h"
#include "idle.h"

#include <glib.h>

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

/**
 * No-op dummy.
 */
void
idle_add(G_GNUC_UNUSED unsigned flags)
{
}

/**
 * No-op dummy.
 */
bool
pcm_volume(G_GNUC_UNUSED void *buffer, G_GNUC_UNUSED size_t length,
	   G_GNUC_UNUSED enum sample_format format,
	   G_GNUC_UNUSED int volume)
{
	return true;
}

void
decoder_initialized(G_GNUC_UNUSED struct decoder *decoder,
		    G_GNUC_UNUSED const struct audio_format *audio_format,
		    G_GNUC_UNUSED bool seekable,
		    G_GNUC_UNUSED float total_time)
{
}

enum decoder_command
decoder_get_command(G_GNUC_UNUSED struct decoder *decoder)
{
	return DECODE_COMMAND_NONE;
}

void decoder_command_finished(G_GNUC_UNUSED struct decoder *decoder)
{
}

double decoder_seek_where(G_GNUC_UNUSED struct decoder *decoder)
{
	return 1.0;
}

void decoder_seek_error(G_GNUC_UNUSED struct decoder *decoder)
{
}

size_t
decoder_read(G_GNUC_UNUSED struct decoder *decoder,
	     struct input_stream *is,
	     void *buffer, size_t length)
{
	return input_stream_lock_read(is, buffer, length, NULL);
}

void
decoder_timestamp(G_GNUC_UNUSED struct decoder *decoder,
		  G_GNUC_UNUSED double t)
{
}

enum decoder_command
decoder_data(G_GNUC_UNUSED struct decoder *decoder,
	     G_GNUC_UNUSED struct input_stream *is,
	     const void *data, size_t datalen,
	     G_GNUC_UNUSED uint16_t bit_rate)
{
	G_GNUC_UNUSED ssize_t nbytes = write(1, data, datalen);
	return DECODE_COMMAND_NONE;
}

enum decoder_command
decoder_tag(G_GNUC_UNUSED struct decoder *decoder,
	    G_GNUC_UNUSED struct input_stream *is,
	    G_GNUC_UNUSED const struct tag *tag)
{
	return DECODE_COMMAND_NONE;
}

float
decoder_replay_gain(G_GNUC_UNUSED struct decoder *decoder,
		    G_GNUC_UNUSED const struct replay_gain_info *replay_gain_info)
{
	return 0.0;
}

void
decoder_mixramp(G_GNUC_UNUSED struct decoder *decoder,
		G_GNUC_UNUSED float replay_gain_db,
		char *mixramp_start, char *mixramp_end)
{
	g_free(mixramp_start);
	g_free(mixramp_end);
}

static bool empty = true;

static void
print_duration(unsigned seconds, G_GNUC_UNUSED void *ctx)
{
	g_print("duration=%d\n", seconds);
}

static void
print_tag(enum tag_type type, const char *value, G_GNUC_UNUSED void *ctx)
{
	g_print("[%s]=%s\n", tag_item_names[type], value);
	empty = false;
}

static void
print_pair(const char *name, const char *value, G_GNUC_UNUSED void *ctx)
{
	g_print("\"%s\"=%s\n", name, value);
}

static const struct tag_handler print_handler = {
	.duration = print_duration,
	.tag = print_tag,
	.pair = print_pair,
};

int main(int argc, char **argv)
{
	GError *error = NULL;
	const char *decoder_name, *path;
	const struct decoder_plugin *plugin;

#ifdef HAVE_LOCALE_H
	/* initialize locale */
	setlocale(LC_CTYPE,"");
#endif

	if (argc != 3) {
		g_printerr("Usage: read_tags DECODER FILE\n");
		return 1;
	}

	decoder_name = argv[1];
	path = argv[2];

	g_thread_init(NULL);
	io_thread_init();
	if (!io_thread_start(&error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return EXIT_FAILURE;
	}

	if (!input_stream_global_init(&error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return 2;
	}

	decoder_plugin_init_all();

	plugin = decoder_plugin_from_name(decoder_name);
	if (plugin == NULL) {
		g_printerr("No such decoder: %s\n", decoder_name);
		return 1;
	}

	bool success = decoder_plugin_scan_file(plugin, path,
						&print_handler, NULL);
	if (!success && plugin->scan_stream != NULL) {
		GMutex *mutex = g_mutex_new();
		GCond *cond = g_cond_new();

		struct input_stream *is =
			input_stream_open(path, mutex, cond, &error);

		if (is == NULL) {
			g_printerr("Failed to open %s: %s\n",
				   path, error->message);
			g_error_free(error);
			return 1;
		}

		success = decoder_plugin_scan_stream(plugin, is,
						     &print_handler, NULL);
		input_stream_close(is);

		g_cond_free(cond);
		g_mutex_free(mutex);
	}

	decoder_plugin_deinit_all();
	input_stream_global_finish();
	io_thread_deinit();

	if (!success) {
		g_printerr("Failed to read tags\n");
		return 1;
	}

	if (empty) {
		tag_ape_scan2(path, &print_handler, NULL);
		if (empty)
			tag_id3_scan(path, &print_handler, NULL);
	}

	return 0;
}
