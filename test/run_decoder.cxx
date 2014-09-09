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

struct Decoder {
	const char *uri;

	const struct DecoderPlugin *plugin;

	bool initialized;
};

void
decoder_initialized(Decoder &decoder,
		    const AudioFormat audio_format,
		    gcc_unused bool seekable,
		    float duration)
{
	struct audio_format_string af_string;

	assert(!decoder.initialized);
	assert(audio_format.IsValid());

	g_printerr("audio_format=%s duration=%f\n",
		   audio_format_to_string(audio_format, &af_string),
		   duration);

	decoder.initialized = true;
}

DecoderCommand
decoder_get_command(gcc_unused Decoder &decoder)
{
	return DecoderCommand::NONE;
}

void
decoder_command_finished(gcc_unused Decoder &decoder)
{
}

double
decoder_seek_where(gcc_unused Decoder &decoder)
{
	return 1.0;
}

void
decoder_seek_error(gcc_unused Decoder &decoder)
{
}

size_t
decoder_read(gcc_unused Decoder *decoder,
	     InputStream &is,
	     void *buffer, size_t length)
{
	return is.LockRead(buffer, length, IgnoreError());
}

bool
decoder_read_full(Decoder *decoder, InputStream &is,
		  void *_buffer, size_t size)
{
	uint8_t *buffer = (uint8_t *)_buffer;

	while (size > 0) {
		size_t nbytes = decoder_read(decoder, is, buffer, size);
		if (nbytes == 0)
			return false;

		buffer += nbytes;
		size -= nbytes;
	}

	return true;
}

bool
decoder_skip(Decoder *decoder, InputStream &is, size_t size)
{
	while (size > 0) {
		char buffer[1024];
		size_t nbytes = decoder_read(decoder, is, buffer,
					     std::min(sizeof(buffer), size));
		if (nbytes == 0)
			return false;

		size -= nbytes;
	}

	return true;
}

void
decoder_timestamp(gcc_unused Decoder &decoder,
		  gcc_unused double t)
{
}

DecoderCommand
decoder_data(gcc_unused Decoder &decoder,
	     gcc_unused InputStream *is,
	     const void *data, size_t datalen,
	     gcc_unused uint16_t kbit_rate)
{
	gcc_unused ssize_t nbytes = write(1, data, datalen);
	return DecoderCommand::NONE;
}

DecoderCommand
decoder_tag(gcc_unused Decoder &decoder,
	    gcc_unused InputStream *is,
	    gcc_unused Tag &&tag)
{
	return DecoderCommand::NONE;
}

void
decoder_replay_gain(gcc_unused Decoder &decoder,
		    const ReplayGainInfo *rgi)
{
	const ReplayGainTuple *tuple = &rgi->tuples[REPLAY_GAIN_ALBUM];
	if (tuple->IsDefined())
		g_printerr("replay_gain[album]: gain=%f peak=%f\n",
			   tuple->gain, tuple->peak);

	tuple = &rgi->tuples[REPLAY_GAIN_TRACK];
	if (tuple->IsDefined())
		g_printerr("replay_gain[track]: gain=%f peak=%f\n",
			   tuple->gain, tuple->peak);
}

void
decoder_mixramp(gcc_unused Decoder &decoder, MixRampInfo &&mix_ramp)
{
	fprintf(stderr, "MixRamp: start='%s' end='%s'\n",
		mix_ramp.GetStart(), mix_ramp.GetEnd());
}

int main(int argc, char **argv)
{
	const char *decoder_name;

	if (argc != 3) {
		g_printerr("Usage: run_decoder DECODER URI >OUT\n");
		return 1;
	}

	Decoder decoder;
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
		decoder.plugin->FileDecode(decoder, decoder.uri);
	} else if (decoder.plugin->stream_decode != NULL) {
		Mutex mutex;
		Cond cond;

		InputStream *is =
			InputStream::Open(decoder.uri, mutex, cond, error);
		if (is == NULL) {
			if (error.IsDefined())
				LogError(error);
			else
				g_printerr("InputStream::Open() failed\n");

			return 1;
		}

		decoder.plugin->StreamDecode(decoder, *is);

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
