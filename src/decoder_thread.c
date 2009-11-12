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
#include "decoder_thread.h"
#include "decoder_control.h"
#include "decoder_internal.h"
#include "decoder_list.h"
#include "decoder_plugin.h"
#include "input_stream.h"
#include "player_control.h"
#include "pipe.h"
#include "song.h"
#include "tag.h"
#include "mapper.h"
#include "path.h"
#include "uri.h"

#include <glib.h>

#include <unistd.h>

static enum decoder_command
decoder_lock_get_command(struct decoder_control *dc)
{
	enum decoder_command command;

	decoder_lock(dc);
	command = dc->command;
	decoder_unlock(dc);

	return command;
}

/**
 * Opens the input stream with input_stream_open(), and waits until
 * the stream gets ready.  If a decoder STOP command is received
 * during that, it cancels the operation (but does not close the
 * stream).
 *
 * Unlock the decoder before calling this function.
 *
 * @return true on success of if #DECODE_COMMAND_STOP is received,
 * false on error
 */
static bool
decoder_input_stream_open(struct decoder_control *dc,
			  struct input_stream *is, const char *uri)
{
	if (!input_stream_open(is, uri))
		return false;

	/* wait for the input stream to become ready; its metadata
	   will be available then */

	while (!is->ready &&
	       decoder_lock_get_command(dc) != DECODE_COMMAND_STOP) {
		int ret;

		ret = input_stream_buffer(is);
		if (ret < 0) {
			input_stream_close(is);
			return false;
		}
	}

	return true;
}

static bool
decoder_stream_decode(const struct decoder_plugin *plugin,
		      struct decoder *decoder,
		      struct input_stream *input_stream)
{
	assert(plugin != NULL);
	assert(plugin->stream_decode != NULL);
	assert(decoder != NULL);
	assert(decoder->stream_tag == NULL);
	assert(decoder->decoder_tag == NULL);
	assert(input_stream != NULL);
	assert(input_stream->ready);
	assert(decoder->dc->state == DECODE_STATE_START);

	if (decoder->dc->command == DECODE_COMMAND_STOP)
		return true;

	decoder_unlock(decoder->dc);

	/* rewind the stream, so each plugin gets a fresh start */
	input_stream_seek(input_stream, 0, SEEK_SET);

	decoder_plugin_stream_decode(plugin, decoder, input_stream);

	decoder_lock(decoder->dc);

	assert(decoder->dc->state == DECODE_STATE_START ||
	       decoder->dc->state == DECODE_STATE_DECODE);

	return decoder->dc->state != DECODE_STATE_START;
}

static bool
decoder_file_decode(const struct decoder_plugin *plugin,
		    struct decoder *decoder, const char *path)
{
	assert(plugin != NULL);
	assert(plugin->file_decode != NULL);
	assert(decoder != NULL);
	assert(decoder->stream_tag == NULL);
	assert(decoder->decoder_tag == NULL);
	assert(path != NULL);
	assert(g_path_is_absolute(path));
	assert(decoder->dc->state == DECODE_STATE_START);

	if (decoder->dc->command == DECODE_COMMAND_STOP)
		return true;

	decoder_unlock(decoder->dc);

	decoder_plugin_file_decode(plugin, decoder, path);

	decoder_lock(decoder->dc);

	assert(decoder->dc->state == DECODE_STATE_START ||
	       decoder->dc->state == DECODE_STATE_DECODE);

	return decoder->dc->state != DECODE_STATE_START;
}

/**
 * Try decoding a stream, using plugins matching the stream's MIME type.
 */
static bool
decoder_run_stream_mime_type(struct decoder *decoder, struct input_stream *is)
{
	const struct decoder_plugin *plugin;
	unsigned int next = 0;

	if (is->mime == NULL)
		return false;

	while ((plugin = decoder_plugin_from_mime_type(is->mime, next++)))
		if (plugin->stream_decode != NULL &&
		    decoder_stream_decode(plugin, decoder, is))
			return true;

	return false;
}

/**
 * Try decoding a stream, using plugins matching the stream's URI
 * suffix.
 */
static bool
decoder_run_stream_suffix(struct decoder *decoder, struct input_stream *is,
			  const char *uri)
{
	const char *suffix = uri_get_suffix(uri);
	const struct decoder_plugin *plugin = NULL;

	if (suffix == NULL)
		return false;

	while ((plugin = decoder_plugin_from_suffix(suffix, plugin)) != NULL)
		if (plugin->stream_decode != NULL &&
		    decoder_stream_decode(plugin, decoder, is))
			return true;

	return false;
}

/**
 * Try decoding a stream, using the fallback plugin.
 */
static bool
decoder_run_stream_fallback(struct decoder *decoder, struct input_stream *is)
{
	const struct decoder_plugin *plugin;

	plugin = decoder_plugin_from_name("mad");
	return plugin != NULL && plugin->stream_decode != NULL &&
		decoder_stream_decode(plugin, decoder, is);
}

/**
 * Try decoding a stream.
 */
static bool
decoder_run_stream(struct decoder *decoder, const char *uri)
{
	struct decoder_control *dc = decoder->dc;
	struct input_stream input_stream;
	bool success;

	decoder_unlock(dc);

	if (!decoder_input_stream_open(dc, &input_stream, uri)) {
		decoder_lock(dc);
		return false;
	}

	decoder_lock(dc);

	success = dc->command == DECODE_COMMAND_STOP ||
		/* first we try mime types: */
		decoder_run_stream_mime_type(decoder, &input_stream) ||
		/* if that fails, try suffix matching the URL: */
		decoder_run_stream_suffix(decoder, &input_stream, uri) ||
		/* fallback to mp3: this is needed for bastard streams
		   that don't have a suffix or set the mimeType */
		decoder_run_stream_fallback(decoder, &input_stream);

	decoder_unlock(dc);
	input_stream_close(&input_stream);
	decoder_lock(dc);

	return success;
}

/**
 * Try decoding a file.
 */
static bool
decoder_run_file(struct decoder *decoder, const char *path_fs)
{
	struct decoder_control *dc = decoder->dc;
	const char *suffix = uri_get_suffix(path_fs);
	const struct decoder_plugin *plugin = NULL;

	if (suffix == NULL)
		return false;

	decoder_unlock(dc);

	while ((plugin = decoder_plugin_from_suffix(suffix, plugin)) != NULL) {
		if (plugin->file_decode != NULL) {
			decoder_lock(dc);

			if (decoder_file_decode(plugin, decoder, path_fs))
				return true;

			decoder_unlock(dc);
		} else if (plugin->stream_decode != NULL) {
			struct input_stream input_stream;
			bool success;

			if (!decoder_input_stream_open(dc, &input_stream,
						       path_fs))
				continue;

			decoder_lock(dc);

			success = decoder_stream_decode(plugin, decoder,
							&input_stream);

			decoder_unlock(dc);

			input_stream_close(&input_stream);

			if (success) {
				decoder_lock(dc);
				return true;
			}
		}
	}

	decoder_lock(dc);
	return false;
}

static void
decoder_run_song(struct decoder_control *dc,
		 const struct song *song, const char *uri)
{
	struct decoder decoder = {
		.dc = dc,
	};
	int ret;

	decoder.seeking = false;
	decoder.song_tag = song->tag != NULL && song_is_file(song)
		? tag_dup(song->tag) : NULL;
	decoder.stream_tag = NULL;
	decoder.decoder_tag = NULL;
	decoder.chunk = NULL;

	dc->state = DECODE_STATE_START;
	dc->command = DECODE_COMMAND_NONE;

	player_signal();

	pcm_convert_init(&decoder.conv_state);

	ret = song_is_file(song)
		? decoder_run_file(&decoder, uri)
		: decoder_run_stream(&decoder, uri);

	decoder_unlock(dc);

	pcm_convert_deinit(&decoder.conv_state);

	/* flush the last chunk */
	if (decoder.chunk != NULL)
		decoder_flush_chunk(&decoder);

	if (decoder.song_tag != NULL)
		tag_free(decoder.song_tag);

	if (decoder.stream_tag != NULL)
		tag_free(decoder.stream_tag);

	if (decoder.decoder_tag != NULL)
		tag_free(decoder.decoder_tag);

	decoder_lock(dc);

	dc->state = ret ? DECODE_STATE_STOP : DECODE_STATE_ERROR;
}

static void
decoder_run(struct decoder_control *dc)
{
	const struct song *song = dc->song;
	char *uri;

	assert(song != NULL);

	if (song_is_file(song))
		uri = map_song_fs(song);
	else
		uri = song_get_uri(song);

	if (uri == NULL) {
		dc->state = DECODE_STATE_ERROR;
		return;
	}

	decoder_run_song(dc, song, uri);
	g_free(uri);

}

static gpointer
decoder_task(gpointer arg)
{
	struct decoder_control *dc = arg;

	decoder_lock(dc);

	do {
		assert(dc->state == DECODE_STATE_STOP ||
		       dc->state == DECODE_STATE_ERROR);

		switch (dc->command) {
		case DECODE_COMMAND_START:
		case DECODE_COMMAND_SEEK:
			decoder_run(dc);

			dc->command = DECODE_COMMAND_NONE;

			player_signal();
			break;

		case DECODE_COMMAND_STOP:
			dc->command = DECODE_COMMAND_NONE;

			player_signal();
			break;

		case DECODE_COMMAND_NONE:
			decoder_wait(dc);
			break;
		}
	} while (dc->command != DECODE_COMMAND_NONE || !dc->quit);

	decoder_unlock(dc);

	return NULL;
}

void
decoder_thread_start(struct decoder_control *dc)
{
	GError *e = NULL;

	assert(dc->thread == NULL);

	dc->quit = false;

	dc->thread = g_thread_create(decoder_task, dc, true, &e);
	if (dc->thread == NULL)
		g_error("Failed to spawn decoder task: %s", e->message);
}
