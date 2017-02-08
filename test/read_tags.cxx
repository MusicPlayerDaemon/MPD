/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "tag/Handler.hxx"
#include "tag/Generic.hxx"
#include "fs/Path.hxx"
#include "thread/Cond.hxx"
#include "Log.hxx"
#include "util/ScopeExit.hxx"

#include <stdexcept>

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

static constexpr TagHandler print_handler = {
	print_duration,
	print_tag,
	print_pair,
};

int main(int argc, char **argv)
try {
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

	const ScopeIOThread io_thread;

	input_stream_global_init(io_thread_get());
	AtScopeExit() { input_stream_global_finish(); };

	decoder_plugin_init_all();
	AtScopeExit() { decoder_plugin_deinit_all(); };

	plugin = decoder_plugin_from_name(decoder_name);
	if (plugin == NULL) {
		fprintf(stderr, "No such decoder: %s\n", decoder_name);
		return EXIT_FAILURE;
	}

	bool success;
	try {
		success = plugin->ScanFile(path, print_handler, nullptr);
	} catch (const std::exception &e) {
		LogError(e);
		success = false;
	}

	Mutex mutex;
	Cond cond;
	InputStreamPtr is;

	if (!success && plugin->scan_stream != NULL) {
		is = InputStream::OpenReady(path.c_str(), mutex, cond);
		success = plugin->ScanStream(*is, print_handler, nullptr);
	}

	if (!success) {
		fprintf(stderr, "Failed to read tags\n");
		return EXIT_FAILURE;
	}

	if (empty) {
		if (is)
			ScanGenericTags(*is, print_handler, nullptr);
		else
			ScanGenericTags(path, print_handler, nullptr);
	}

	return 0;
} catch (const std::exception &e) {
	LogError(e);
	return EXIT_FAILURE;
}
