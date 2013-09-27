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
#include "TagSave.hxx"
#include "Song.hxx"
#include "SongEnumerator.hxx"
#include "Directory.hxx"
#include "InputStream.hxx"
#include "ConfigGlobal.hxx"
#include "DecoderAPI.hxx"
#include "DecoderList.hxx"
#include "InputInit.hxx"
#include "IOThread.hxx"
#include "PlaylistRegistry.hxx"
#include "PlaylistPlugin.hxx"
#include "fs/Path.hxx"
#include "util/Error.hxx"

#include <glib.h>

#include <unistd.h>
#include <stdlib.h>

Directory::Directory() {}
Directory::~Directory() {}

static void
my_log_func(const gchar *log_domain, gcc_unused GLogLevelFlags log_level,
	    const gchar *message, gcc_unused gpointer user_data)
{
	if (log_domain != NULL)
		g_printerr("%s: %s\n", log_domain, message);
	else
		g_printerr("%s\n", message);
}

void
decoder_initialized(gcc_unused struct decoder *decoder,
		    gcc_unused const AudioFormat audio_format,
		    gcc_unused bool seekable,
		    gcc_unused float total_time)
{
}

DecoderCommand
decoder_get_command(gcc_unused struct decoder *decoder)
{
	return DecoderCommand::NONE;
}

void
decoder_command_finished(gcc_unused struct decoder *decoder)
{
}

double
decoder_seek_where(gcc_unused struct decoder *decoder)
{
	return 1.0;
}

void
decoder_seek_error(gcc_unused struct decoder *decoder)
{
}

size_t
decoder_read(gcc_unused struct decoder *decoder,
	     struct input_stream *is,
	     void *buffer, size_t length)
{
	Error error;
	return is->LockRead(buffer, length, error);
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
	const char *uri;
	struct input_stream *is = NULL;
	Song *song;

	if (argc != 3) {
		g_printerr("Usage: dump_playlist CONFIG URI\n");
		return 1;
	}

	const Path config_path = Path::FromFS(argv[1]);
	uri = argv[2];

	/* initialize GLib */

#if !GLIB_CHECK_VERSION(2,32,0)
	g_thread_init(NULL);
#endif

	g_log_set_default_handler(my_log_func, NULL);

	/* initialize MPD */

	config_global_init();

	Error error;
	if (!ReadConfigFile(config_path, error)) {
		g_printerr("%s\n", error.GetMessage());
		return 1;
	}

	io_thread_init();
	io_thread_start();

	if (!input_stream_global_init(error)) {
		g_warning("%s", error.GetMessage());
		return 2;
	}

	playlist_list_global_init();
	decoder_plugin_init_all();

	/* open the playlist */

	Mutex mutex;
	Cond cond;

	auto playlist = playlist_list_open_uri(uri, mutex, cond);
	if (playlist == NULL) {
		/* open the stream and wait until it becomes ready */

		is = input_stream::Open(uri, mutex, cond, error);
		if (is == NULL) {
			if (error.IsDefined())
				g_warning("%s", error.GetMessage());
			else
				g_printerr("input_stream::Open() failed\n");
			return 2;
		}

		is->LockWaitReady();

		/* open the playlist */

		playlist = playlist_list_open_stream(is, uri);
		if (playlist == NULL) {
			is->Close();
			g_printerr("Failed to open playlist\n");
			return 2;
		}
	}

	/* dump the playlist */

	while ((song = playlist->NextSong()) != NULL) {
		g_print("%s\n", song->uri);

		if (song->end_ms > 0)
			g_print("range: %u:%02u..%u:%02u\n",
				song->start_ms / 60000,
				(song->start_ms / 1000) % 60,
				song->end_ms / 60000,
				(song->end_ms / 1000) % 60);
		else if (song->start_ms > 0)
			g_print("range: %u:%02u..\n",
				song->start_ms / 60000,
				(song->start_ms / 1000) % 60);

		if (song->tag != NULL)
			tag_save(stdout, *song->tag);

		song->Free();
	}

	/* deinitialize everything */

	delete playlist;
	if (is != NULL)
		is->Close();

	decoder_plugin_deinit_all();
	playlist_list_global_finish();
	input_stream_global_finish();
	io_thread_deinit();
	config_global_finish();

	return 0;
}
