/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "DecoderThread.hxx"
#include "DecoderControl.hxx"
#include "DecoderInternal.hxx"
#include "decoder_error.h"
#include "decoder_plugin.h"
#include "song.h"
#include "mpd_error.h"
#include "Mapper.hxx"
#include "Path.hxx"
#include "decoder_api.h"
#include "tag.h"
#include "input_stream.h"

extern "C" {
#include "decoder_list.h"
#include "replay_gain_ape.h"
#include "uri.h"
}

#include <glib.h>

#include <unistd.h>
#include <stdio.h> /* for SEEK_SET */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "decoder_thread"

/**
 * Marks the current decoder command as "finished" and notifies the
 * player thread.
 *
 * @param dc the #decoder_control object; must be locked
 */
static void
decoder_command_finished_locked(struct decoder_control *dc)
{
	assert(dc->command != DECODE_COMMAND_NONE);

	dc->command = DECODE_COMMAND_NONE;

	g_cond_signal(dc->client_cond);
}

/**
 * Opens the input stream with input_stream_open(), and waits until
 * the stream gets ready.  If a decoder STOP command is received
 * during that, it cancels the operation (but does not close the
 * stream).
 *
 * Unlock the decoder before calling this function.
 *
 * @return an input_stream on success or if #DECODE_COMMAND_STOP is
 * received, NULL on error
 */
static struct input_stream *
decoder_input_stream_open(struct decoder_control *dc, const char *uri)
{
	GError *error = NULL;
	struct input_stream *is;

	is = input_stream_open(uri, dc->mutex, dc->cond, &error);
	if (is == NULL) {
		if (error != NULL) {
			g_warning("%s", error->message);
			g_error_free(error);
		}

		return NULL;
	}

	/* wait for the input stream to become ready; its metadata
	   will be available then */

	decoder_lock(dc);

	input_stream_update(is);
	while (!is->ready &&
	       dc->command != DECODE_COMMAND_STOP) {
		decoder_wait(dc);

		input_stream_update(is);
	}

	if (!input_stream_check(is, &error)) {
		decoder_unlock(dc);

		g_warning("%s", error->message);
		g_error_free(error);

		return NULL;
	}

	decoder_unlock(dc);

	return is;
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

	g_debug("probing plugin %s", plugin->name);

	if (decoder->dc->command == DECODE_COMMAND_STOP)
		return true;

	/* rewind the stream, so each plugin gets a fresh start */
	input_stream_seek(input_stream, 0, SEEK_SET, NULL);

	decoder_unlock(decoder->dc);

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

	g_debug("probing plugin %s", plugin->name);

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
 * Hack to allow tracking const decoder plugins in a GSList.
 */
static inline gpointer
deconst_plugin(const struct decoder_plugin *plugin)
{
	return const_cast<struct decoder_plugin *>(plugin);
}

/**
 * Try decoding a stream, using plugins matching the stream's MIME type.
 *
 * @param tried_r a list of plugins which were tried
 */
static bool
decoder_run_stream_mime_type(struct decoder *decoder, struct input_stream *is,
			     GSList **tried_r)
{
	assert(tried_r != NULL);

	const struct decoder_plugin *plugin;
	unsigned int next = 0;

	if (is->mime == NULL)
		return false;

	while ((plugin = decoder_plugin_from_mime_type(is->mime, next++))) {
		if (plugin->stream_decode == NULL)
			continue;

		if (g_slist_find(*tried_r, plugin) != NULL)
			/* don't try a plugin twice */
			continue;

		if (decoder_stream_decode(plugin, decoder, is))
			return true;

		*tried_r = g_slist_prepend(*tried_r, deconst_plugin(plugin));
	}

	return false;
}

/**
 * Try decoding a stream, using plugins matching the stream's URI
 * suffix.
 *
 * @param tried_r a list of plugins which were tried
 */
static bool
decoder_run_stream_suffix(struct decoder *decoder, struct input_stream *is,
			  const char *uri, GSList **tried_r)
{
	assert(tried_r != NULL);

	const char *suffix = uri_get_suffix(uri);
	const struct decoder_plugin *plugin = NULL;

	if (suffix == NULL)
		return false;

	while ((plugin = decoder_plugin_from_suffix(suffix, plugin)) != NULL) {
		if (plugin->stream_decode == NULL)
			continue;

		if (g_slist_find(*tried_r, plugin) != NULL)
			/* don't try a plugin twice */
			continue;

		if (decoder_stream_decode(plugin, decoder, is))
			return true;

		*tried_r = g_slist_prepend(*tried_r, deconst_plugin(plugin));
	}

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
	struct input_stream *input_stream;
	bool success;

	decoder_unlock(dc);

	input_stream = decoder_input_stream_open(dc, uri);
	if (input_stream == NULL) {
		decoder_lock(dc);
		return false;
	}

	decoder_lock(dc);

	GSList *tried = NULL;

	success = dc->command == DECODE_COMMAND_STOP ||
		/* first we try mime types: */
		decoder_run_stream_mime_type(decoder, input_stream, &tried) ||
		/* if that fails, try suffix matching the URL: */
		decoder_run_stream_suffix(decoder, input_stream, uri,
					  &tried) ||
		/* fallback to mp3: this is needed for bastard streams
		   that don't have a suffix or set the mimeType */
		(tried == NULL &&
		 decoder_run_stream_fallback(decoder, input_stream));

	g_slist_free(tried);

	decoder_unlock(dc);
	input_stream_close(input_stream);
	decoder_lock(dc);

	return success;
}

/**
 * Attempt to load replay gain data, and pass it to
 * decoder_replay_gain().
 */
static void
decoder_load_replay_gain(struct decoder *decoder, const char *path_fs)
{
	struct replay_gain_info info;
	if (replay_gain_ape_read(path_fs, &info))
		decoder_replay_gain(decoder, &info);
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

	decoder_load_replay_gain(decoder, path_fs);

	while ((plugin = decoder_plugin_from_suffix(suffix, plugin)) != NULL) {
		if (plugin->file_decode != NULL) {
			decoder_lock(dc);

			if (decoder_file_decode(plugin, decoder, path_fs))
				return true;

			decoder_unlock(dc);
		} else if (plugin->stream_decode != NULL) {
			struct input_stream *input_stream;
			bool success;

			input_stream = decoder_input_stream_open(dc, path_fs);
			if (input_stream == NULL)
				continue;

			decoder_lock(dc);

			success = decoder_stream_decode(plugin, decoder,
							input_stream);

			decoder_unlock(dc);

			input_stream_close(input_stream);

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
	decoder decoder(dc, dc->start_ms > 0,
			song->tag != NULL && song_is_file(song)
			? tag_dup(song->tag) : nullptr);
	int ret;

	dc->state = DECODE_STATE_START;

	decoder_command_finished_locked(dc);

	ret = song_is_file(song)
		? decoder_run_file(&decoder, uri)
		: decoder_run_stream(&decoder, uri);

	decoder_unlock(dc);

	/* flush the last chunk */

	if (decoder.chunk != NULL)
		decoder_flush_chunk(&decoder);

	decoder_lock(dc);

	if (ret)
		dc->state = DECODE_STATE_STOP;
	else {
		dc->state = DECODE_STATE_ERROR;

		const char *error_uri = song->uri;
		char *allocated = uri_remove_auth(error_uri);
		if (allocated != NULL)
			error_uri = allocated;

		dc->error = g_error_new(decoder_quark(), 0,
					"Failed to decode %s", error_uri);
		g_free(allocated);
	}

	g_cond_signal(dc->client_cond);
}

static void
decoder_run(struct decoder_control *dc)
{
	dc_clear_error(dc);

	const struct song *song = dc->song;
	char *uri;

	assert(song != NULL);

	if (song_is_file(song))
		uri = map_song_fs(song).Steal();
	else
		uri = song_get_uri(song);

	if (uri == NULL) {
		dc->state = DECODE_STATE_ERROR;
		dc->error = g_error_new(decoder_quark(), 0,
					"Failed to map song");

		decoder_command_finished_locked(dc);
		return;
	}

	decoder_run_song(dc, song, uri);
	g_free(uri);

}

static gpointer
decoder_task(gpointer arg)
{
	struct decoder_control *dc = (struct decoder_control *)arg;

	decoder_lock(dc);

	do {
		assert(dc->state == DECODE_STATE_STOP ||
		       dc->state == DECODE_STATE_ERROR);

		switch (dc->command) {
		case DECODE_COMMAND_START:
			dc_mixramp_start(dc, NULL);
			dc_mixramp_prev_end(dc, dc->mixramp_end);
			dc->mixramp_end = NULL; /* Don't free, it's copied above. */
			dc->replay_gain_prev_db = dc->replay_gain_db;
			dc->replay_gain_db = 0;

                        /* fall through */

		case DECODE_COMMAND_SEEK:
			decoder_run(dc);
			break;

		case DECODE_COMMAND_STOP:
			decoder_command_finished_locked(dc);
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
		MPD_ERROR("Failed to spawn decoder task: %s", e->message);
}
