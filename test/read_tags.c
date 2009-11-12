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
#include "decoder_list.h"
#include "decoder_api.h"
#include "input_stream.h"
#include "audio_format.h"
#include "pcm_volume.h"
#include "tag_ape.h"
#include "tag_id3.h"
#include "idle.h"

#include <glib.h>

#include <assert.h>
#include <unistd.h>

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
pcm_volume(G_GNUC_UNUSED void *buffer, G_GNUC_UNUSED int length,
	   G_GNUC_UNUSED const struct audio_format *format,
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

char *decoder_get_uri(G_GNUC_UNUSED struct decoder *decoder)
{
	return NULL;
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
	return input_stream_read(is, buffer, length);
}

enum decoder_command
decoder_data(G_GNUC_UNUSED struct decoder *decoder,
	     G_GNUC_UNUSED struct input_stream *is,
	     const void *data, size_t datalen,
	     G_GNUC_UNUSED float data_time, G_GNUC_UNUSED uint16_t bit_rate,
	     G_GNUC_UNUSED struct replay_gain_info *replay_gain_info)
{
	write(1, data, datalen);
	return DECODE_COMMAND_NONE;
}

enum decoder_command
decoder_tag(G_GNUC_UNUSED struct decoder *decoder,
	    G_GNUC_UNUSED struct input_stream *is,
	    G_GNUC_UNUSED const struct tag *tag)
{
	return DECODE_COMMAND_NONE;
}

static void
print_tag(const struct tag *tag)
{
	if (tag->time >= 0)
		g_print("time=%d\n", tag->time);

	for (unsigned i = 0; i < tag->num_items; ++i)
		g_print("%s=%s\n",
			tag_item_names[tag->items[i]->type],
			tag->items[i]->value);
}

int main(int argc, char **argv)
{
	const char *decoder_name, *path;
	const struct decoder_plugin *plugin;
	struct tag *tag;
	bool empty;

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

	input_stream_global_init();
	decoder_plugin_init_all();

	plugin = decoder_plugin_from_name(decoder_name);
	if (plugin == NULL) {
		g_printerr("No such decoder: %s\n", decoder_name);
		return 1;
	}

	tag = decoder_plugin_tag_dup(plugin, path);
	decoder_plugin_deinit_all();
	input_stream_global_finish();
	if (tag == NULL) {
		g_printerr("Failed to read tags\n");
		return 1;
	}

	print_tag(tag);

	empty = tag_is_empty(tag);
	tag_free(tag);

	if (empty) {
		tag = tag_ape_load(path);
		if (tag == NULL)
			tag = tag_id3_load(path);
		if (tag != NULL) {
			print_tag(tag);
			tag_free(tag);
		}
	}

	return 0;
}
