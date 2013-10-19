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
#include "IOThread.hxx"
#include "DecoderList.hxx"
#include "DecoderAPI.hxx"
#include "InputInit.hxx"
#include "InputStream.hxx"
#include "AudioFormat.hxx"
#include "util/Error.hxx"
#include "thread/Cond.hxx"
#include "Log.hxx"
#include "stdbin.h"

#include <glib.h>

#include <assert.h>
#include <unistd.h>
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

struct decoder {
	const char *uri;

	const struct decoder_plugin *plugin;

	bool initialized;
};

void
decoder_initialized(struct decoder *decoder,
		    const AudioFormat audio_format,
		    gcc_unused bool seekable,
		    gcc_unused float total_time)
{
	struct audio_format_string af_string;

	assert(!decoder->initialized);
	assert(audio_format.IsValid());

	g_printerr("audio_format=%s\n",
		   audio_format_to_string(audio_format, &af_string));

	decoder->initialized = true;
}

DecoderCommand
decoder_get_command(gcc_unused struct decoder *decoder)
{
	return DecoderCommand::NONE;
}

void decoder_command_finished(gcc_unused struct decoder *decoder)
{
}

double decoder_seek_where(gcc_unused struct decoder *decoder)
{
	return 1.0;
}

void decoder_seek_error(gcc_unused struct decoder *decoder)
{
}

size_t
decoder_read(gcc_unused struct decoder *decoder,
	     struct input_stream *is,
	     void *buffer, size_t length)
{
	return is->LockRead(buffer, length, IgnoreError());
}

void
decoder_timestamp(gcc_unused struct decoder *decoder,
		  gcc_unused double t)
{
}

DecoderCommand
decoder_data(gcc_unused struct decoder *decoder,
	     gcc_unused struct input_stream *is,
	     const void *data, size_t datalen,
	     gcc_unused uint16_t kbit_rate)
{
	gcc_unused ssize_t nbytes = write(1, data, datalen);
	return DecoderCommand::NONE;
}

DecoderCommand
decoder_tag(gcc_unused struct decoder *decoder,
	    gcc_unused struct input_stream *is,
	    gcc_unused Tag &&tag)
{
	return DecoderCommand::NONE;
}

void
decoder_replay_gain(gcc_unused struct decoder *decoder,
		    const struct replay_gain_info *replay_gain_info)
{
	const struct replay_gain_tuple *tuple =
		&replay_gain_info->tuples[REPLAY_GAIN_ALBUM];
	if (replay_gain_tuple_defined(tuple))
		g_printerr("replay_gain[album]: gain=%f peak=%f\n",
			   tuple->gain, tuple->peak);

	tuple = &replay_gain_info->tuples[REPLAY_GAIN_TRACK];
	if (replay_gain_tuple_defined(tuple))
		g_printerr("replay_gain[track]: gain=%f peak=%f\n",
			   tuple->gain, tuple->peak);
}

void
decoder_mixramp(gcc_unused struct decoder *decoder,
		char *mixramp_start, char *mixramp_end)
{
	g_free(mixramp_start);
	g_free(mixramp_end);
}

int main(int argc, char **argv)
{
	const char *decoder_name;
	struct decoder decoder;

	if (argc != 3) {
		g_printerr("Usage: run_decoder DECODER URI >OUT\n");
		return 1;
	}

	decoder_name = argv[1];
	decoder.uri = argv[2];

#if !GLIB_CHECK_VERSION(2,32,0)
	g_thread_init(NULL);
#endif

	g_log_set_default_handler(my_log_func, NULL);

	io_thread_init();
	io_thread_start();

	Error error;
	if (!input_stream_global_init(error)) {
		LogError(error);
		return 2;
	}

	decoder_plugin_init_all();

	decoder.plugin = decoder_plugin_from_name(decoder_name);
	if (decoder.plugin == NULL) {
		g_printerr("No such decoder: %s\n", decoder_name);
		return 1;
	}

	decoder.initialized = false;

	if (decoder.plugin->file_decode != NULL) {
		decoder_plugin_file_decode(*decoder.plugin, &decoder,
					   decoder.uri);
	} else if (decoder.plugin->stream_decode != NULL) {
		Mutex mutex;
		Cond cond;

		input_stream *is =
			input_stream::Open(decoder.uri, mutex, cond, error);
		if (is == NULL) {
			if (error.IsDefined())
				LogError(error);
			else
				g_printerr("input_stream::Open() failed\n");

			return 1;
		}

		decoder_plugin_stream_decode(*decoder.plugin, &decoder, is);

		is->Close();
	} else {
		g_printerr("Decoder plugin is not usable\n");
		return 1;
	}

	decoder_plugin_deinit_all();
	input_stream_global_finish();
	io_thread_deinit();

	if (!decoder.initialized) {
		g_printerr("Decoding failed\n");
		return 1;
	}

	return 0;
}
