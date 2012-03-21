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
#include "decoder_api.h"
#include "decoder_internal.h"
#include "decoder_control.h"
#include "audio_config.h"
#include "song.h"
#include "buffer.h"
#include "pipe.h"
#include "chunk.h"
#include "replay_gain_config.h"

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
	g_cond_signal(dc->client_cond);
	decoder_unlock(dc);

	g_debug("audio_format=%s, seekable=%s",
		audio_format_to_string(&dc->in_audio_format, &af_string),
		seekable ? "true" : "false");

	if (!audio_format_equals(&dc->in_audio_format,
				 &dc->out_audio_format))
		g_debug("converting to %s",
			audio_format_to_string(&dc->out_audio_format,
					       &af_string));
}

/**
 * Checks if we need an "initial seek".  If so, then the initial seek
 * is prepared, and the function returns true.
 */
G_GNUC_PURE
static bool
decoder_prepare_initial_seek(struct decoder *decoder)
{
	const struct decoder_control *dc = decoder->dc;
	assert(dc->pipe != NULL);

	if (dc->state != DECODE_STATE_DECODE)
		/* wait until the decoder has finished initialisation
		   (reading file headers etc.) before emitting the
		   virtual "SEEK" command */
		return false;

	if (decoder->initial_seek_running)
		/* initial seek has already begun - override any other
		   command */
		return true;

	if (decoder->initial_seek_pending) {
		if (!dc->seekable) {
			/* seeking is not possible */
			decoder->initial_seek_pending = false;
			return false;
		}

		if (dc->command == DECODE_COMMAND_NONE) {
			/* begin initial seek */

			decoder->initial_seek_pending = false;
			decoder->initial_seek_running = true;
			return true;
		}

		/* skip initial seek when there's another command
		   (e.g. STOP) */

		decoder->initial_seek_pending = false;
	}

	return false;
}

/**
 * Returns the current decoder command.  May return a "virtual"
 * synthesized command, e.g. to seek to the beginning of the CUE
 * track.
 */
G_GNUC_PURE
static enum decoder_command
decoder_get_virtual_command(struct decoder *decoder)
{
	const struct decoder_control *dc = decoder->dc;
	assert(dc->pipe != NULL);

	if (decoder_prepare_initial_seek(decoder))
		return DECODE_COMMAND_SEEK;

	return dc->command;
}

enum decoder_command
decoder_get_command(struct decoder *decoder)
{
	return decoder_get_virtual_command(decoder);
}

void
decoder_command_finished(struct decoder *decoder)
{
	struct decoder_control *dc = decoder->dc;

	decoder_lock(dc);

	assert(dc->command != DECODE_COMMAND_NONE ||
	       decoder->initial_seek_running);
	assert(dc->command != DECODE_COMMAND_SEEK ||
	       decoder->initial_seek_running ||
	       dc->seek_error || decoder->seeking);
	assert(dc->pipe != NULL);

	if (decoder->initial_seek_running) {
		assert(!decoder->seeking);
		assert(decoder->chunk == NULL);
		assert(music_pipe_empty(dc->pipe));

		decoder->initial_seek_running = false;
		decoder->timestamp = dc->start_ms / 1000.;
		decoder_unlock(dc);
		return;
	}

	if (decoder->seeking) {
		decoder->seeking = false;

		/* delete frames from the old song position */

		if (decoder->chunk != NULL) {
			music_buffer_return(dc->buffer, decoder->chunk);
			decoder->chunk = NULL;
		}

		music_pipe_clear(dc->pipe, dc->buffer);

		decoder->timestamp = dc->seek_where;
	}

	dc->command = DECODE_COMMAND_NONE;
	g_cond_signal(dc->client_cond);
	decoder_unlock(dc);
}

double decoder_seek_where(G_GNUC_UNUSED struct decoder * decoder)
{
	const struct decoder_control *dc = decoder->dc;

	assert(dc->pipe != NULL);

	if (decoder->initial_seek_running)
		return dc->start_ms / 1000.;

	assert(dc->command == DECODE_COMMAND_SEEK);

	decoder->seeking = true;

	return dc->seek_where;
}

void decoder_seek_error(struct decoder * decoder)
{
	struct decoder_control *dc = decoder->dc;

	assert(dc->pipe != NULL);

	if (decoder->initial_seek_running) {
		/* d'oh, we can't seek to the sub-song start position,
		   what now? - no idea, ignoring the problem for now. */
		decoder->initial_seek_running = false;
		return;
	}

	assert(dc->command == DECODE_COMMAND_SEEK);

	dc->seek_error = true;
	decoder->seeking = false;

	decoder_command_finished(decoder);
}

/**
 * Should be read operation be cancelled?  That is the case when the
 * player thread has sent a command such as "STOP".
 */
G_GNUC_PURE
static inline bool
decoder_check_cancel_read(const struct decoder *decoder)
{
	if (decoder == NULL)
		return false;

	const struct decoder_control *dc = decoder->dc;
	if (dc->command == DECODE_COMMAND_NONE)
		return false;

	/* ignore the SEEK command during initialization, the plugin
	   should handle that after it has initialized successfully */
	if (dc->command == DECODE_COMMAND_SEEK &&
	    (dc->state == DECODE_STATE_START || decoder->seeking))
		return false;

	return true;
}

size_t decoder_read(struct decoder *decoder,
		    struct input_stream *is,
		    void *buffer, size_t length)
{
	/* XXX don't allow decoder==NULL */
	GError *error = NULL;
	size_t nbytes;

	assert(decoder == NULL ||
	       decoder->dc->state == DECODE_STATE_START ||
	       decoder->dc->state == DECODE_STATE_DECODE);
	assert(is != NULL);
	assert(buffer != NULL);

	if (length == 0)
		return 0;

	input_stream_lock(is);

	while (true) {
		if (decoder_check_cancel_read(decoder)) {
			input_stream_unlock(is);
			return 0;
		}

		if (input_stream_available(is))
			break;

		g_cond_wait(is->cond, is->mutex);
	}

	nbytes = input_stream_read(is, buffer, length, &error);
	assert(nbytes == 0 || error == NULL);
	assert(nbytes > 0 || error != NULL || input_stream_eof(is));

	if (G_UNLIKELY(nbytes == 0 && error != NULL)) {
		g_warning("%s", error->message);
		g_error_free(error);
	}

	input_stream_unlock(is);

	return nbytes;
}

void
decoder_timestamp(struct decoder *decoder, double t)
{
	assert(decoder != NULL);
	assert(t >= 0);

	decoder->timestamp = t;
}

/**
 * Sends a #tag as-is to the music pipe.  Flushes the current chunk
 * (decoder.chunk) if there is one.
 */
static enum decoder_command
do_send_tag(struct decoder *decoder, const struct tag *tag)
{
	struct music_chunk *chunk;

	if (decoder->chunk != NULL) {
		/* there is a partial chunk - flush it, we want the
		   tag in a new chunk */
		decoder_flush_chunk(decoder);
		g_cond_signal(decoder->dc->client_cond);
	}

	assert(decoder->chunk == NULL);

	chunk = decoder_get_chunk(decoder);
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
		? input_stream_lock_tag(is)
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
	     uint16_t kbit_rate)
{
	struct decoder_control *dc = decoder->dc;
	const char *data = _data;
	GError *error = NULL;
	enum decoder_command cmd;

	assert(dc->state == DECODE_STATE_DECODE);
	assert(dc->pipe != NULL);
	assert(length % audio_format_frame_size(&dc->in_audio_format) == 0);

	decoder_lock(dc);
	cmd = decoder_get_virtual_command(decoder);
	decoder_unlock(dc);

	if (cmd == DECODE_COMMAND_STOP || cmd == DECODE_COMMAND_SEEK ||
	    length == 0)
		return cmd;

	/* send stream tags */

	if (update_stream_tag(decoder, is)) {
		if (decoder->decoder_tag != NULL) {
			/* merge with tag from decoder plugin */
			struct tag *tag;

			tag = tag_merge(decoder->decoder_tag,
					decoder->stream_tag);
			cmd = do_send_tag(decoder, tag);
			tag_free(tag);
		} else
			/* send only the stream tag */
			cmd = do_send_tag(decoder, decoder->stream_tag);

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

		chunk = decoder_get_chunk(decoder);
		if (chunk == NULL) {
			assert(dc->command != DECODE_COMMAND_NONE);
			return dc->command;
		}

		dest = music_chunk_write(chunk, &dc->out_audio_format,
					 decoder->timestamp -
					 dc->song->start_ms / 1000.0,
					 kbit_rate, &nbytes);
		if (dest == NULL) {
			/* the chunk is full, flush it */
			decoder_flush_chunk(decoder);
			g_cond_signal(dc->client_cond);
			continue;
		}

		assert(nbytes > 0);

		if (nbytes > length)
			nbytes = length;

		/* copy the buffer */

		memcpy(dest, data, nbytes);

		/* expand the music pipe chunk */

		full = music_chunk_expand(chunk, &dc->out_audio_format, nbytes);
		if (full) {
			/* the chunk is full, flush it */
			decoder_flush_chunk(decoder);
			g_cond_signal(dc->client_cond);
		}

		data += nbytes;
		length -= nbytes;

		decoder->timestamp += (double)nbytes /
			audio_format_time_to_size(&dc->out_audio_format);

		if (dc->end_ms > 0 &&
		    decoder->timestamp >= dc->end_ms / 1000.0)
			/* the end of this range has been reached:
			   stop decoding */
			return DECODE_COMMAND_STOP;
	}

	return DECODE_COMMAND_NONE;
}

enum decoder_command
decoder_tag(G_GNUC_UNUSED struct decoder *decoder, struct input_stream *is,
	    const struct tag *tag)
{
	G_GNUC_UNUSED const struct decoder_control *dc = decoder->dc;
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

	/* check if we're seeking */

	if (decoder_prepare_initial_seek(decoder))
		/* during initial seek, no music chunk must be created
		   until seeking is finished; skip the rest of the
		   function here */
		return DECODE_COMMAND_SEEK;

	/* send tag to music pipe */

	if (decoder->stream_tag != NULL) {
		/* merge with tag from input stream */
		struct tag *merged;

		merged = tag_merge(decoder->stream_tag, decoder->decoder_tag);
		cmd = do_send_tag(decoder, merged);
		tag_free(merged);
	} else
		/* send only the decoder tag */
		cmd = do_send_tag(decoder, tag);

	return cmd;
}

float
decoder_replay_gain(struct decoder *decoder,
		    const struct replay_gain_info *replay_gain_info)
{
	float return_db = 0;
	assert(decoder != NULL);

	if (replay_gain_info != NULL) {
		static unsigned serial;
		if (++serial == 0)
			serial = 1;

		if (REPLAY_GAIN_OFF != replay_gain_mode) {
			return_db = 20.0 * log10f(
				replay_gain_tuple_scale(
					&replay_gain_info->tuples[replay_gain_get_real_mode()],
					replay_gain_preamp, replay_gain_missing_preamp,
					replay_gain_limit));
		}

		decoder->replay_gain_info = *replay_gain_info;
		decoder->replay_gain_serial = serial;

		if (decoder->chunk != NULL) {
			/* flush the current chunk because the new
			   replay gain values affect the following
			   samples */
			decoder_flush_chunk(decoder);
			g_cond_signal(decoder->dc->client_cond);
		}
	} else
		decoder->replay_gain_serial = 0;

	return return_db;
}

void
decoder_mixramp(struct decoder *decoder, float replay_gain_db,
		char *mixramp_start, char *mixramp_end)
{
	assert(decoder != NULL);
	struct decoder_control *dc = decoder->dc;
	assert(dc != NULL);

	dc->replay_gain_db = replay_gain_db;
	dc_mixramp_start(dc, mixramp_start);
	dc_mixramp_end(dc, mixramp_end);
}
