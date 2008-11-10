/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "decoder_thread.h"
#include "decoder_control.h"
#include "decoder_internal.h"
#include "player_control.h"
#include "pipe.h"
#include "song.h"
#include "mapper.h"
#include "path.h"
#include "log.h"
#include "ls.h"

static bool
decoder_stream_decode(const struct decoder_plugin *plugin,
		      struct decoder *decoder,
		      struct input_stream *input_stream)
{
	bool ret;

	assert(plugin != NULL);
	assert(plugin->stream_decode != NULL);
	assert(decoder != NULL);
	assert(!decoder->stream_tag_sent);
	assert(input_stream != NULL);
	assert(input_stream->ready);
	assert(dc.state == DECODE_STATE_START);

	/* rewind the stream, so each plugin gets a fresh start */
	input_stream_seek(input_stream, 0, SEEK_SET);

	ret = plugin->stream_decode(decoder, input_stream);

	if (ret) {
		/* if the method has succeeded, the plugin must have
		   called decoder_initialized() */
		assert(dc.state == DECODE_STATE_DECODE);
	} else {
		/* no decoder_initialized() allowed when the plugin
		   hasn't recognized the file format */
		assert(dc.state == DECODE_STATE_START);
	}

	return ret;
}

static bool
decoder_file_decode(const struct decoder_plugin *plugin,
		    struct decoder *decoder, const char *path)
{
	bool ret;

	assert(plugin != NULL);
	assert(plugin->stream_decode != NULL);
	assert(decoder != NULL);
	assert(!decoder->stream_tag_sent);
	assert(path != NULL);
	assert(path[0] == '/');
	assert(dc.state == DECODE_STATE_START);

	ret = plugin->file_decode(decoder, path);

	if (ret) {
		/* if the method has succeeded, the plugin must have
		   called decoder_initialized() */
		assert(dc.state == DECODE_STATE_DECODE);
	} else {
		/* no decoder_initialized() allowed when the plugin
		   hasn't recognized the file format */
		assert(dc.state == DECODE_STATE_START);
	}

	return ret;
}

static void decoder_run(void)
{
	struct song *song = dc.next_song;
	char buffer[MPD_PATH_MAX];
	const char *uri;
	struct decoder decoder;
	int ret;
	bool close_instream = true;
	struct input_stream input_stream;
	const struct decoder_plugin *plugin;

	if (song_is_file(song))
		uri = map_song_fs(song, buffer);
	else
		uri = song_get_url(song, buffer);
	if (uri == NULL) {
		dc.state = DECODE_STATE_ERROR;
		return;
	}

	dc.current_song = dc.next_song; /* NEED LOCK */
	if (!input_stream_open(&input_stream, uri)) {
		dc.state = DECODE_STATE_ERROR;
		return;
	}

	decoder.seeking = false;
	decoder.stream_tag_sent = false;

	dc.state = DECODE_STATE_START;
	dc.command = DECODE_COMMAND_NONE;
	notify_signal(&pc.notify);

	/* wait for the input stream to become ready; its metadata
	   will be available then */

	while (!input_stream.ready) {
		if (dc.command != DECODE_COMMAND_NONE) {
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
		input_stream_close(&input_stream);
		dc.state = DECODE_STATE_STOP;
		return;
	}

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
			const char *s = getSuffix(uri);
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
			if ((plugin = decoder_plugin_from_name("mp3"))) {
				ret = decoder_stream_decode(plugin, &decoder,
							    &input_stream);
			}
		}
	} else {
		unsigned int next = 0;
		const char *s = getSuffix(uri);
		while ((plugin = decoder_plugin_from_suffix(s, next++))) {
			if (plugin->file_decode != NULL) {
				input_stream_close(&input_stream);
				close_instream = false;
				ret = decoder_file_decode(plugin,
							  &decoder, uri);
				if (ret)
					break;
			} else if (plugin->stream_decode != NULL) {
				ret = decoder_stream_decode(plugin, &decoder,
							    &input_stream);
				if (ret)
					break;
			}
		}
	}

	music_pipe_flush();

	if (close_instream)
		input_stream_close(&input_stream);

	dc.state = ret ? DECODE_STATE_STOP : DECODE_STATE_ERROR;
}

static void * decoder_task(mpd_unused void *arg)
{
	while (1) {
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
	}

	return NULL;
}

void decoder_thread_start(void)
{
	pthread_attr_t attr;
	pthread_t decoder_thread;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&decoder_thread, &attr, decoder_task, NULL))
		FATAL("Failed to spawn decoder task: %s\n", strerror(errno));
}
