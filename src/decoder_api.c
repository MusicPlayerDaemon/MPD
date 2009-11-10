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
#include "decoder_api.h"
#include "decoder_internal.h"
#include "decoder_control.h"
#include "player_control.h"
#include "audio.h"
#include "song.h"
#include "buffer.h"

#include "normalize.h"
#include "pipe.h"
#include "chunk.h"

#include <glib.h>

#include <assert.h>
#include <stdlib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "decoder"

void
decoder_initialized(struct decoder *decoder,
		    const struct audio_format *audio_format,
		    bool seekable, float total_time)
{
	struct decoder_control *dc = decoder->dc;
	struct audio_format_string af_string;

	assert(dc->state == DECODE_STATE_START);
	assert(dc->pipe != NULL);
	assert(decoder != NULL);
	assert(decoder->stream_tag == NULL);
	assert(decoder->decoder_tag == NULL);
	assert(!decoder->seeking);
	assert(audio_format != NULL);
	assert(audio_format_defined(audio_format));
	assert(audio_format_valid(audio_format));

	dc->in_audio_format = *audio_format;
	getOutputAudioFormat(audio_format, &dc->out_audio_format);

	dc->seekable = seekable;
	dc->total_time = total_time;

	decoder_lock(dc);
	dc->state = DECODE_STATE_DECODE;
	decoder_unlock(dc);

	player_lock_signal();

	g_debug("audio_format=%s, seekable=%s",
		audio_format_to_string(&dc->in_audio_format, &af_string),
		seekable ? "true" : "false");

	if (!audio_format_equals(&dc->in_audio_format,
				 &dc->out_audio_format))
		g_debug("converting to %s",
			audio_format_to_string(&dc->out_audio_format,
					       &af_string));
}

char *decoder_get_uri(G_GNUC_UNUSED struct decoder *decoder)
{
	const struct decoder_control *dc = decoder->dc;

	assert(dc->pipe != NULL);

	return song_get_uri(dc->song);
}

enum decoder_command decoder_get_command(G_GNUC_UNUSED struct decoder * decoder)
{
	const struct decoder_control *dc = decoder->dc;

	assert(dc->pipe != NULL);

	return dc->command;
}

void decoder_command_finished(G_GNUC_UNUSED struct decoder * decoder)
{
	struct decoder_control *dc = decoder->dc;

	decoder_lock(dc);

	assert(dc->command != DECODE_COMMAND_NONE);
	assert(dc->command != DECODE_COMMAND_SEEK ||
	       dc->seek_error || decoder->seeking);
	assert(dc->pipe != NULL);

	if (dc->command == DECODE_COMMAND_SEEK) {
		/* delete frames from the old song position */

		if (decoder->chunk != NULL) {
			music_buffer_return(dc->buffer, decoder->chunk);
			decoder->chunk = NULL;
		}

		music_pipe_clear(dc->pipe, dc->buffer);
	}

	dc->command = DECODE_COMMAND_NONE;
	decoder_unlock(dc);

	player_lock_signal();
}

double decoder_seek_where(G_GNUC_UNUSED struct decoder * decoder)
{
	const struct decoder_control *dc = decoder->dc;

	assert(dc->command == DECODE_COMMAND_SEEK);
	assert(dc->pipe != NULL);

	decoder->seeking = true;

	return dc->seek_where;
}

void decoder_seek_error(struct decoder * decoder)
{
	struct decoder_control *dc = decoder->dc;

	assert(dc->command == DECODE_COMMAND_SEEK);
	assert(dc->pipe != NULL);

	dc->seek_error = true;
	decoder_command_finished(decoder);
}

size_t decoder_read(struct decoder *decoder,
		    struct input_stream *is,
		    void *buffer, size_t length)
{
	const struct decoder_control *dc =
		decoder != NULL ? decoder->dc : NULL;
	size_t nbytes;

	assert(decoder == NULL ||
	       dc->state == DECODE_STATE_START ||
	       dc->state == DECODE_STATE_DECODE);
	assert(is != NULL);
	assert(buffer != NULL);

	if (length == 0)
		return 0;

	while (true) {
		/* XXX don't allow decoder==NULL */
		if (decoder != NULL &&
		    /* ignore the SEEK command during initialization,
		       the plugin should handle that after it has
		       initialized successfully */
		    (dc->command != DECODE_COMMAND_SEEK ||
		     (dc->state != DECODE_STATE_START && !decoder->seeking)) &&
		    dc->command != DECODE_COMMAND_NONE)
			return 0;

		nbytes = input_stream_read(is, buffer, length);
		if (nbytes > 0 || input_stream_eof(is))
			return nbytes;

		/* sleep for a fraction of a second! */
		/* XXX don't sleep, wait for an event instead */
		g_usleep(10000);
	}
}

/**
 * Sends a #tag as-is to the music pipe.  Flushes the current chunk
 * (decoder.chunk) if there is one.
 */
static enum decoder_command
do_send_tag(struct decoder *decoder, struct input_stream *is,
	    const struct tag *tag)
{
	struct music_chunk *chunk;

	if (decoder->chunk != NULL) {
		/* there is a partial chunk - flush it, we want the
		   tag in a new chunk */
		decoder_flush_chunk(decoder);
		player_lock_signal();
	}

	assert(decoder->chunk == NULL);

	chunk = decoder_get_chunk(decoder, is);
	if (chunk == NULL) {
		assert(decoder->dc->command != DECODE_COMMAND_NONE);
		return decoder->dc->command;
	}

	chunk->tag = tag_dup(tag);
	return DECODE_COMMAND_NONE;
}

static bool
update_stream_tag(struct decoder *decoder, struct input_stream *is)
{
	struct tag *tag;

	tag = is != NULL
		? input_stream_tag(is)
		: NULL;
	if (tag == NULL) {
		tag = decoder->song_tag;
		if (tag == NULL)
			return false;

		/* no stream tag present - submit the song tag
		   instead */
		decoder->song_tag = NULL;
	}

	if (decoder->stream_tag != NULL)
		tag_free(decoder->stream_tag);

	decoder->stream_tag = tag;
	return true;
}

enum decoder_command
decoder_data(struct decoder *decoder,
	     struct input_stream *is,
	     const void *_data, size_t length,
	     float data_time, uint16_t bitRate,
	     struct replay_gain_info *replay_gain_info)
{
	struct decoder_control *dc = decoder->dc;
	const char *data = _data;
	GError *error = NULL;
	enum decoder_command cmd;

	assert(dc->state == DECODE_STATE_DECODE);
	assert(dc->pipe != NULL);
	assert(length % audio_format_frame_size(&dc->in_audio_format) == 0);

	decoder_lock(dc);
	cmd = dc->command;
	decoder_unlock(dc);

	if (cmd == DECODE_COMMAND_STOP || cmd == DECODE_COMMAND_SEEK ||
	    length == 0)
		return cmd;

	/* send stream tags */

	if (update_stream_tag(decoder, is)) {
		if (decoder->decoder_tag != NULL) {
			/* merge with tag from decoder plugin */
			struct tag *tag;

			tag = tag_merge(decoder->stream_tag,
					decoder->decoder_tag);
			cmd = do_send_tag(decoder, is, tag);
			tag_free(tag);
		} else
			/* send only the stream tag */
			cmd = do_send_tag(decoder, is, decoder->stream_tag);

		if (cmd != DECODE_COMMAND_NONE)
			return cmd;
	}

	if (!audio_format_equals(&dc->in_audio_format, &dc->out_audio_format)) {
		data = pcm_convert(&decoder->conv_state,
				   &dc->in_audio_format, data, length,
				   &dc->out_audio_format, &length,
				   &error);
		if (data == NULL) {
			/* the PCM conversion has failed - stop
			   playback, since we have no better way to
			   bail out */
			g_warning("%s", error->message);
			return DECODE_COMMAND_STOP;
		}
	}

	while (length > 0) {
		struct music_chunk *chunk;
		char *dest;
		size_t nbytes;
		bool full;

		chunk = decoder_get_chunk(decoder, is);
		if (chunk == NULL) {
			assert(dc->command != DECODE_COMMAND_NONE);
			return dc->command;
		}

		dest = music_chunk_write(chunk, &dc->out_audio_format,
					 data_time, bitRate, &nbytes);
		if (dest == NULL) {
			/* the chunk is full, flush it */
			decoder_flush_chunk(decoder);
			player_lock_signal();
			continue;
		}

		assert(nbytes > 0);

		if (nbytes > length)
			nbytes = length;

		/* copy the buffer */

		memcpy(dest, data, nbytes);

		/* apply replay gain or normalization */

		if (replay_gain_mode != REPLAY_GAIN_OFF)
			replay_gain_apply(replay_gain_info, dest, nbytes,
					  &dc->out_audio_format);
		else if (normalizationEnabled)
			normalizeData(dest, nbytes, &dc->out_audio_format);

		/* expand the music pipe chunk */

		full = music_chunk_expand(chunk, &dc->out_audio_format, nbytes);
		if (full) {
			/* the chunk is full, flush it */
			decoder_flush_chunk(decoder);
			player_lock_signal();
		}

		data += nbytes;
		length -= nbytes;
	}

	return DECODE_COMMAND_NONE;
}

enum decoder_command
decoder_tag(G_GNUC_UNUSED struct decoder *decoder, struct input_stream *is,
	    const struct tag *tag)
{
	const struct decoder_control *dc = decoder->dc;
	enum decoder_command cmd;

	assert(dc->state == DECODE_STATE_DECODE);
	assert(dc->pipe != NULL);
	assert(tag != NULL);

	/* save the tag */

	if (decoder->decoder_tag != NULL)
		tag_free(decoder->decoder_tag);
	decoder->decoder_tag = tag_dup(tag);

	/* check for a new stream tag */

	update_stream_tag(decoder, is);

	/* send tag to music pipe */

	if (decoder->stream_tag != NULL) {
		/* merge with tag from input stream */
		struct tag *merged;

		merged = tag_merge(decoder->stream_tag, decoder->decoder_tag);
		cmd = do_send_tag(decoder, is, merged);
		tag_free(merged);
	} else
		/* send only the decoder tag */
		cmd = do_send_tag(decoder, is, tag);

	return cmd;
}
