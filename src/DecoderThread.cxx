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
#include "DecoderError.hxx"
#include "DecoderPlugin.hxx"
#include "Song.hxx"
#include "system/FatalError.hxx"
#include "Mapper.hxx"
#include "fs/Path.hxx"
#include "DecoderAPI.hxx"
#include "tag/Tag.hxx"
#include "InputStream.hxx"
#include "DecoderList.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "tag/ApeReplayGain.hxx"

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
	assert(dc->command != DecoderCommand::NONE);

	dc->command = DecoderCommand::NONE;

	dc->client_cond.signal();
}

/**
 * Opens the input stream with input_stream::Open(), and waits until
 * the stream gets ready.  If a decoder STOP command is received
 * during that, it cancels the operation (but does not close the
 * stream).
 *
 * Unlock the decoder before calling this function.
 *
 * @return an input_stream on success or if #DecoderCommand::STOP is
 * received, NULL on error
 */
static struct input_stream *
decoder_input_stream_open(struct decoder_control *dc, const char *uri)
{
	Error error;

	input_stream *is = input_stream::Open(uri, dc->mutex, dc->cond, error);
	if (is == NULL) {
		if (error.IsDefined())
			g_warning("%s", error.GetMessage());

		return NULL;
	}

	/* wait for the input stream to become ready; its metadata
	   will be available then */

	dc->Lock();

	is->Update();
	while (!is->ready &&
	       dc->command != DecoderCommand::STOP) {
		dc->Wait();

		is->Update();
	}

	if (!is->Check(error)) {
		dc->Unlock();

		g_warning("%s", error.GetMessage());
		return NULL;
	}

	dc->Unlock();

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

	if (decoder->dc->command == DecoderCommand::STOP)
		return true;

	/* rewind the stream, so each plugin gets a fresh start */
	input_stream->Seek(0, SEEK_SET, IgnoreError());

	decoder->dc->Unlock();

	decoder_plugin_stream_decode(plugin, decoder, input_stream);

	decoder->dc->Lock();

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

	if (decoder->dc->command == DecoderCommand::STOP)
		return true;

	decoder->dc->Unlock();

	decoder_plugin_file_decode(plugin, decoder, path);

	decoder->dc->Lock();

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

	if (is->mime.empty())
		return false;

	while ((plugin = decoder_plugin_from_mime_type(is->mime.c_str(),
						       next++))) {
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

	dc->Unlock();

	input_stream = decoder_input_stream_open(dc, uri);
	if (input_stream == NULL) {
		dc->Lock();
		return false;
	}

	dc->Lock();

	GSList *tried = NULL;

	success = dc->command == DecoderCommand::STOP ||
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

	dc->Unlock();
	input_stream->Close();
	dc->Lock();

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

	dc->Unlock();

	decoder_load_replay_gain(decoder, path_fs);

	while ((plugin = decoder_plugin_from_suffix(suffix, plugin)) != NULL) {
		if (plugin->file_decode != NULL) {
			dc->Lock();

			if (decoder_file_decode(plugin, decoder, path_fs))
				return true;

			dc->Unlock();
		} else if (plugin->stream_decode != NULL) {
			struct input_stream *input_stream;
			bool success;

			input_stream = decoder_input_stream_open(dc, path_fs);
			if (input_stream == NULL)
				continue;

			dc->Lock();

			success = decoder_stream_decode(plugin, decoder,
							input_stream);

			dc->Unlock();

			input_stream->Close();

			if (success) {
				dc->Lock();
				return true;
			}
		}
	}

	dc->Lock();
	return false;
}

static void
decoder_run_song(struct decoder_control *dc,
		 const Song *song, const char *uri)
{
	decoder decoder(dc, dc->start_ms > 0,
			song->tag != NULL && song->IsFile()
			? new Tag(*song->tag) : nullptr);
	int ret;

	dc->state = DECODE_STATE_START;

	decoder_command_finished_locked(dc);

	ret = song->IsFile()
		? decoder_run_file(&decoder, uri)
		: decoder_run_stream(&decoder, uri);

	dc->Unlock();

	/* flush the last chunk */

	if (decoder.chunk != NULL)
		decoder_flush_chunk(&decoder);

	dc->Lock();

	if (ret)
		dc->state = DECODE_STATE_STOP;
	else {
		dc->state = DECODE_STATE_ERROR;

		const char *error_uri = song->uri;
		char *allocated = uri_remove_auth(error_uri);
		if (allocated != NULL)
			error_uri = allocated;

		dc->error.Format(decoder_domain,
				 "Failed to decode %s", error_uri);
		g_free(allocated);
	}

	dc->client_cond.signal();
}

static void
decoder_run(struct decoder_control *dc)
{
	dc->ClearError();

	const Song *song = dc->song;
	char *uri;

	assert(song != NULL);

	if (song->IsFile())
		uri = map_song_fs(song).Steal();
	else
		uri = song->GetURI();

	if (uri == NULL) {
		dc->state = DECODE_STATE_ERROR;
		dc->error.Set(decoder_domain, "Failed to map song");

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

	dc->Lock();

	do {
		assert(dc->state == DECODE_STATE_STOP ||
		       dc->state == DECODE_STATE_ERROR);

		switch (dc->command) {
		case DecoderCommand::START:
			dc->MixRampStart(nullptr);
			dc->MixRampPrevEnd(dc->mixramp_end);
			dc->mixramp_end = NULL; /* Don't free, it's copied above. */
			dc->replay_gain_prev_db = dc->replay_gain_db;
			dc->replay_gain_db = 0;

                        /* fall through */

		case DecoderCommand::SEEK:
			decoder_run(dc);
			break;

		case DecoderCommand::STOP:
			decoder_command_finished_locked(dc);
			break;

		case DecoderCommand::NONE:
			dc->Wait();
			break;
		}
	} while (dc->command != DecoderCommand::NONE || !dc->quit);

	dc->Unlock();

	return NULL;
}

void
decoder_thread_start(struct decoder_control *dc)
{
	assert(dc->thread == NULL);

	dc->quit = false;

#if GLIB_CHECK_VERSION(2,32,0)
	dc->thread = g_thread_new("thread", decoder_task, dc);
#else
	GError *e = NULL;
	dc->thread = g_thread_create(decoder_task, dc, true, &e);
	if (dc->thread == NULL)
		FatalError("Failed to spawn decoder task", e);
#endif
}
