/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "ScopeIOThread.hxx"
#include "decoder/DecoderList.hxx"
#include "decoder/DecoderPlugin.hxx"
#include "FakeDecoderAPI.hxx"
#include "input/Init.hxx"
#include "input/InputStream.hxx"
#include "fs/Path.hxx"
#include "AudioFormat.hxx"
#include "util/Error.hxx"
#include "Log.hxx"
#include "stdbin.h"

#ifdef HAVE_GLIB
#include <glib.h>
#endif

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "Usage: run_decoder DECODER URI >OUT\n");
		return EXIT_FAILURE;
	}

	Decoder decoder;
	const char *const decoder_name = argv[1];
	const char *const uri = argv[2];

#ifdef HAVE_GLIB
#if !GLIB_CHECK_VERSION(2,32,0)
	g_thread_init(NULL);
#endif
#endif

	const ScopeIOThread io_thread;

	Error error;
	if (!input_stream_global_init(error)) {
		LogError(error);
		return EXIT_FAILURE;
	}

	decoder_plugin_init_all();

	const DecoderPlugin *plugin = decoder_plugin_from_name(decoder_name);
	if (plugin == nullptr) {
		fprintf(stderr, "No such decoder: %s\n", decoder_name);
		return EXIT_FAILURE;
	}

	if (plugin->file_decode != nullptr) {
		plugin->FileDecode(decoder, Path::FromFS(uri));
	} else if (plugin->stream_decode != nullptr) {
		InputStream *is =
			InputStream::OpenReady(uri, decoder.mutex,
					       decoder.cond, error);
		if (is == NULL) {
			if (error.IsDefined())
				LogError(error);
			else
				fprintf(stderr, "InputStream::Open() failed\n");

			return EXIT_FAILURE;
		}

		plugin->StreamDecode(decoder, *is);

		delete is;
	} else {
		fprintf(stderr, "Decoder plugin is not usable\n");
		return EXIT_FAILURE;
	}

	decoder_plugin_deinit_all();
	input_stream_global_finish();

	if (!decoder.initialized) {
		fprintf(stderr, "Decoding failed\n");
		return EXIT_FAILURE;
	}

	return 0;
}
