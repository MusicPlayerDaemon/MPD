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
#include "input/Init.hxx"
#include "input/InputStream.hxx"
#include "AudioFormat.hxx"
#include "tag/TagHandler.hxx"
#include "tag/TagId3.hxx"
#include "tag/ApeTag.hxx"
#include "util/Error.hxx"
#include "fs/Path.hxx"
#include "thread/Cond.hxx"
#include "Log.hxx"

#ifdef HAVE_GLIB
#include <glib.h>
#endif

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

static bool empty = true;

static void
print_duration(SongTime duration, gcc_unused void *ctx)
{
	printf("duration=%f\n", duration.ToDoubleS());
}

static void
print_tag(TagType type, const char *value, gcc_unused void *ctx)
{
	printf("[%s]=%s\n", tag_item_names[type], value);
	empty = false;
}

static void
print_pair(const char *name, const char *value, gcc_unused void *ctx)
{
	printf("\"%s\"=%s\n", name, value);
}

static const struct tag_handler print_handler = {
	print_duration,
	print_tag,
	print_pair,
};

int main(int argc, char **argv)
{
	const char *decoder_name;
	const struct DecoderPlugin *plugin;

#ifdef HAVE_LOCALE_H
	/* initialize locale */
	setlocale(LC_CTYPE,"");
#endif

	if (argc != 3) {
		fprintf(stderr, "Usage: read_tags DECODER FILE\n");
		return EXIT_FAILURE;
	}

	decoder_name = argv[1];
	const Path path = Path::FromFS(argv[2]);

#ifdef HAVE_GLIB
#if !GLIB_CHECK_VERSION(2,32,0)
	g_thread_init(NULL);
#endif
#endif

	const ScopeIOThread io_thread;

	Error error;
	if (!input_stream_global_init(error)) {
		LogError(error);
		return 2;
	}

	decoder_plugin_init_all();

	plugin = decoder_plugin_from_name(decoder_name);
	if (plugin == NULL) {
		fprintf(stderr, "No such decoder: %s\n", decoder_name);
		return EXIT_FAILURE;
	}

	bool success = plugin->ScanFile(path, print_handler, nullptr);
	if (!success && plugin->scan_stream != NULL) {
		Mutex mutex;
		Cond cond;

		InputStream *is = InputStream::OpenReady(path.c_str(),
							 mutex, cond,
							 error);
		if (is == NULL) {
			FormatError(error, "Failed to open %s", path.c_str());
			return EXIT_FAILURE;
		}

		success = plugin->ScanStream(*is, print_handler, nullptr);
		delete is;
	}

	decoder_plugin_deinit_all();
	input_stream_global_finish();

	if (!success) {
		fprintf(stderr, "Failed to read tags\n");
		return EXIT_FAILURE;
	}

	if (empty) {
		tag_ape_scan2(path, &print_handler, NULL);
		if (empty)
			tag_id3_scan(path, &print_handler, NULL);
	}

	return 0;
}
