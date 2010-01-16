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
	assert(dc.state == DECODE_STATE_START);

	/* rewind the stream, so each plugin gets a fresh start */
	input_stream_seek(input_stream, 0, SEEK_SET);

	decoder_plugin_stream_decode(plugin, decoder, input_stream);

	assert(dc.state == DECODE_STATE_START ||
	       dc.state == DECODE_STATE_DECODE);

	return dc.state != DECODE_STATE_START;
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
	assert(path[0] == '/');
	assert(dc.state == DECODE_STATE_START);

	decoder_plugin_file_decode(plugin, decoder, path);

	assert(dc.state == DECODE_STATE_START ||
	       dc.state == DECODE_STATE_DECODE);

	return dc.state != DECODE_STATE_START;
}

static void decoder_run_song(const struct song *song, const char *uri)
{
	struct decoder decoder;
	int ret;
	bool close_instream = true;
	struct input_stream input_stream;
	const struct decoder_plugin *plugin;

	close_instream = input_stream_open(&input_stream, uri);
	if (!close_instream && !song_is_file(song)) {
		dc.state = DECODE_STATE_ERROR;
		return;
	}

	decoder.seeking = false;
	decoder.song_tag = song->tag != NULL && song_is_file(song)
		? tag_dup(song->tag) : NULL;
	decoder.stream_tag = NULL;
	decoder.decoder_tag = NULL;
	decoder.chunk = NULL;

	dc.state = DECODE_STATE_START;
	dc.command = DECODE_COMMAND_NONE;
	notify_signal(&pc.notify);

	/* wait for the input stream to become ready; its metadata
	   will be available then */

	while (close_instream && !input_stream.ready) {
		if (dc.command == DECODE_COMMAND_STOP) {
			input_stream_close(&input_stream);
			dc.state = DECODE_STATE_STOP;
			return;
		}

		ret = input_stream_buffer(&input_stream);
		if (ret < 0) {
			input_stream_close(&input_stream);
			dc.state = DECODE_STATE_ERROR;
			return;
		}
	}

	if (dc.command == DECODE_COMMAND_STOP) {
		if (close_instream)
			input_stream_close(&input_stream);
		dc.state = DECODE_STATE_STOP;
		return;
	}

	pcm_convert_init(&decoder.conv_state);

	ret = false;
	if (!song_is_file(song)) {
		unsigned int next = 0;

		/* first we try mime types: */
		while ((plugin = decoder_plugin_from_mime_type(input_stream.mime, next++))) {
			if (plugin->stream_decode == NULL)
				continue;
			ret = decoder_stream_decode(plugin, &decoder,
						    &input_stream);
			if (ret)
				break;

			plugin = NULL;
		}

		/* if that fails, try suffix matching the URL: */
		if (plugin == NULL) {
			const char *s = uri_get_suffix(uri);
			next = 0;
			while ((plugin = decoder_plugin_from_suffix(s, next++))) {
				if (plugin->stream_decode == NULL)
					continue;
				ret = decoder_stream_decode(plugin, &decoder,
							    &input_stream);
				if (ret)
					break;

				assert(dc.state == DECODE_STATE_START);
				plugin = NULL;
			}
		}
		/* fallback to mp3: */
		/* this is needed for bastard streams that don't have a suffix
		   or set the mimeType */
		if (plugin == NULL) {
			/* we already know our mp3Plugin supports streams, no
			 * need to check for stream{Types,DecodeFunc} */
			if ((plugin = decoder_plugin_from_name("mad"))) {
				ret = decoder_stream_decode(plugin, &decoder,
							    &input_stream);
			}
		}
	} else {
		unsigned int next = 0;
		const char *s = uri_get_suffix(uri);
		while ((plugin = decoder_plugin_from_suffix(s, next++))) {
			if (plugin->file_decode != NULL) {
				if (close_instream) {
					input_stream_close(&input_stream);
					close_instream = false;
				}

				ret = decoder_file_decode(plugin,
							  &decoder, uri);
				if (ret)
					break;
			} else if (plugin->stream_decode != NULL) {
				if (!close_instream) {
					/* the input_stream object has
					   been closed before
					   decoder_file_decode() -
					   reopen it */
					if (input_stream_open(&input_stream, uri))
						close_instream = true;
					else
						continue;
				}

				ret = decoder_stream_decode(plugin, &decoder,
							    &input_stream);
				if (ret)
					break;
			}
		}
	}

	pcm_convert_deinit(&decoder.conv_state);

	/* flush the last chunk */
	if (decoder.chunk != NULL)
		decoder_flush_chunk(&decoder);

	if (close_instream)
		input_stream_close(&input_stream);

	if (decoder.song_tag != NULL)
		tag_free(decoder.song_tag);

	if (decoder.stream_tag != NULL)
		tag_free(decoder.stream_tag);

	if (decoder.decoder_tag != NULL)
		tag_free(decoder.decoder_tag);

	dc.state = ret ? DECODE_STATE_STOP : DECODE_STATE_ERROR;
}

static void decoder_run(void)
{
	struct song *song = dc.next_song;
	char *uri;

	if (song_is_file(song))
		uri = map_song_fs(song);
	else
		uri = song_get_uri(song);

	if (uri == NULL) {
		dc.state = DECODE_STATE_ERROR;
		return;
	}

	dc.current_song = dc.next_song; /* NEED LOCK */
	decoder_run_song(song, uri);
	g_free(uri);

}

static gpointer decoder_task(G_GNUC_UNUSED gpointer arg)
{
	do {
		assert(dc.state == DECODE_STATE_STOP ||
		       dc.state == DECODE_STATE_ERROR);

		switch (dc.command) {
		case DECODE_COMMAND_START:
		case DECODE_COMMAND_SEEK:
			decoder_run();

			dc.command = DECODE_COMMAND_NONE;
			notify_signal(&pc.notify);
			break;

		case DECODE_COMMAND_STOP:
			dc.command = DECODE_COMMAND_NONE;
			notify_signal(&pc.notify);
			break;

		case DECODE_COMMAND_NONE:
			notify_wait(&dc.notify);
			break;
		}
	} while (dc.command != DECODE_COMMAND_NONE || !dc.quit);

	return NULL;
}

void decoder_thread_start(void)
{
	GError *e = NULL;

	assert(dc.thread == NULL);

	dc.thread = g_thread_create(decoder_task, NULL, true, &e);
	if (dc.thread == NULL)
		g_error("Failed to spawn decoder task: %s", e->message);
}
