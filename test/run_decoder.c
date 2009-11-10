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
#include "idle.h"

#include <glib.h>

#include <assert.h>
#include <unistd.h>

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

struct decoder {
	const char *uri;

	const struct decoder_plugin *plugin;

	bool initialized;
};

void
decoder_initialized(struct decoder *decoder,
		    const struct audio_format *audio_format,
		    G_GNUC_UNUSED bool seekable,
		    G_GNUC_UNUSED float total_time)
{
	struct audio_format_string af_string;

	assert(!decoder->initialized);
	assert(audio_format_valid(audio_format));

	g_printerr("audio_format=%s\n",
		   audio_format_to_string(audio_format, &af_string));

	decoder->initialized = true;
}

char *decoder_get_uri(struct decoder *decoder)
{
	return g_strdup(decoder->uri);
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

int main(int argc, char **argv)
{
	bool ret;
	const char *decoder_name;
	struct decoder decoder;

	if (argc != 3) {
		g_printerr("Usage: run_decoder DECODER URI >OUT\n");
		return 1;
	}

	decoder_name = argv[1];
	decoder.uri = argv[2];

	input_stream_global_init();
	decoder_plugin_init_all();

	decoder.plugin = decoder_plugin_from_name(decoder_name);
	if (decoder.plugin == NULL) {
		g_printerr("No such decoder: %s\n", decoder_name);
		return 1;
	}

	decoder.initialized = false;

	if (decoder.plugin->file_decode != NULL) {
		decoder_plugin_file_decode(decoder.plugin, &decoder,
					   decoder.uri);
	} else if (decoder.plugin->stream_decode != NULL) {
		struct input_stream is;

		ret = input_stream_open(&is, decoder.uri);
		if (!ret)
			return 1;

		decoder_plugin_stream_decode(decoder.plugin, &decoder, &is);

		input_stream_close(&is);
	} else {
		g_printerr("Decoder plugin is not usable\n");
		return 1;
	}

	decoder_plugin_deinit_all();
	input_stream_global_finish();

	if (!decoder.initialized) {
		g_printerr("Decoding failed\n");
		return 1;
	}

	return 0;
}
