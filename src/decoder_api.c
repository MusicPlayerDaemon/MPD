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

#include "decoder_internal.h"
#include "decoder_control.h"
#include "player_control.h"
#include "audio.h"
#include "song.h"

#include "utils.h"
#include "normalize.h"
#include "pipe.h"

#include <glib.h>

#include <assert.h>
#include <stdlib.h>

void decoder_initialized(G_GNUC_UNUSED struct decoder * decoder,
			 const struct audio_format *audio_format,
			 bool seekable, float total_time)
{
	assert(dc.state == DECODE_STATE_START);
	assert(decoder != NULL);
	assert(decoder->stream_tag == NULL);
	assert(decoder->decoder_tag == NULL);
	assert(!decoder->seeking);
	assert(audio_format != NULL);
	assert(audio_format_defined(audio_format));
	assert(audio_format_valid(audio_format));

	dc.in_audio_format = *audio_format;
	getOutputAudioFormat(audio_format, &dc.out_audio_format);

	dc.seekable = seekable;
	dc.total_time = total_time;

	dc.state = DECODE_STATE_DECODE;
	notify_signal(&pc.notify);
}

char *decoder_get_uri(G_GNUC_UNUSED struct decoder *decoder)
{
	return song_get_uri(dc.current_song);
}

enum decoder_command decoder_get_command(G_GNUC_UNUSED struct decoder * decoder)
{
	return dc.command;
}

void decoder_command_finished(G_GNUC_UNUSED struct decoder * decoder)
{
	assert(dc.command != DECODE_COMMAND_NONE);
	assert(dc.command != DECODE_COMMAND_SEEK ||
	       dc.seek_error || decoder->seeking);

	if (dc.command == DECODE_COMMAND_SEEK)
		/* delete frames from the old song position */
		music_pipe_clear();

	dc.command = DECODE_COMMAND_NONE;
	notify_signal(&pc.notify);
}

double decoder_seek_where(G_GNUC_UNUSED struct decoder * decoder)
{
	assert(dc.command == DECODE_COMMAND_SEEK);

	decoder->seeking = true;

	return dc.seek_where;
}

void decoder_seek_error(struct decoder * decoder)
{
	assert(dc.command == DECODE_COMMAND_SEEK);

	dc.seek_error = true;
	decoder_command_finished(decoder);
}

size_t decoder_read(struct decoder *decoder,
		    struct input_stream *is,
		    void *buffer, size_t length)
{
	size_t nbytes;

	assert(decoder == NULL ||
	       dc.state == DECODE_STATE_START ||
	       dc.state == DECODE_STATE_DECODE);
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
		    (dc.command != DECODE_COMMAND_SEEK ||
		     (dc.state != DECODE_STATE_START && !decoder->seeking)) &&
		    dc.command != DECODE_COMMAND_NONE)
			return 0;

		nbytes = input_stream_read(is, buffer, length);
		if (nbytes > 0 || input_stream_eof(is))
			return nbytes;

		/* sleep for a fraction of a second! */
		/* XXX don't sleep, wait for an event instead */
		my_usleep(10000);
	}
}

/**
 * All chunks are full of decoded data; wait for the player to free
 * one.
 */
static enum decoder_command
need_chunks(struct input_stream *is, bool wait)
{
	if (dc.command == DECODE_COMMAND_STOP ||
	    dc.command == DECODE_COMMAND_SEEK)
		return dc.command;

	if ((is == NULL || input_stream_buffer(is) <= 0) && wait) {
		notify_wait(&dc.notify);
		notify_signal(&pc.notify);

		return dc.command;
	}

	return DECODE_COMMAND_NONE;
}

static enum decoder_command
do_send_tag(struct input_stream *is, const struct tag *tag)
{
	while (!music_pipe_tag(tag)) {
		enum decoder_command cmd = need_chunks(is, true);
		if (cmd != DECODE_COMMAND_NONE)
			return cmd;
	}

	return DECODE_COMMAND_NONE;
}

static bool
update_stream_tag(struct decoder *decoder, struct input_stream *is)
{
	struct tag *tag;

	if (is == NULL)
		return false;

	tag = input_stream_tag(is);
	if (tag == NULL)
		return false;

	if (decoder->stream_tag != NULL)
		tag_free(decoder->stream_tag);

	decoder->stream_tag = tag;
	return true;
}

enum decoder_command
decoder_data(struct decoder *decoder,
	     struct input_stream *is,
	     void *_data, size_t length,
	     float data_time, uint16_t bitRate,
	     struct replay_gain_info *replay_gain_info)
{
	static char *conv_buffer;
	static size_t conv_buffer_size;
	size_t nbytes;
	char *data;

	assert(dc.state == DECODE_STATE_DECODE);
	assert(length % audio_format_frame_size(&dc.in_audio_format) == 0);

	if (dc.command == DECODE_COMMAND_STOP ||
	    dc.command == DECODE_COMMAND_SEEK ||
	    length == 0)
		return dc.command;

	/* send stream tags */

	if (update_stream_tag(decoder, is)) {
		enum decoder_command cmd;

		if (decoder->decoder_tag != NULL) {
			/* merge with tag from decoder plugin */
			struct tag *tag;

			tag = tag_merge(decoder->stream_tag,
					decoder->decoder_tag);
			cmd = do_send_tag(is, tag);
			tag_free(tag);
		} else
			/* send only the stream tag */
			cmd = do_send_tag(is, decoder->stream_tag);

		if (cmd != DECODE_COMMAND_NONE)
			return cmd;
	}

	if (audio_format_equals(&dc.in_audio_format, &dc.out_audio_format)) {
		data = _data;
	} else {
		size_t out_length =
			pcm_convert_size(&dc.in_audio_format, length,
					 &dc.out_audio_format);
		if (out_length > conv_buffer_size) {
			g_free(conv_buffer);
			conv_buffer = g_malloc(out_length);
			conv_buffer_size = out_length;
		}

		data = conv_buffer;
		length = pcm_convert(&dc.in_audio_format, _data,
				     length, &dc.out_audio_format,
				     data, &decoder->conv_state);

		/* under certain circumstances, pcm_convert() may
		   return an empty buffer - this condition should be
		   investigated further, but for now, do this check as
		   a workaround: */
		if (length == 0)
			return DECODE_COMMAND_NONE;
	}

	if (replay_gain_info != NULL && (replay_gain_mode != REPLAY_GAIN_OFF))
		replay_gain_apply(replay_gain_info, data, length,
			     &dc.out_audio_format);
	else if (normalizationEnabled)
		normalizeData(data, length, &dc.out_audio_format);

	while (length > 0) {
		nbytes = music_pipe_append(data, length,
					   &dc.out_audio_format,
					   data_time, bitRate);
		length -= nbytes;
		data += nbytes;

		if (length > 0) {
			enum decoder_command cmd =
				need_chunks(is, nbytes == 0);
			if (cmd != DECODE_COMMAND_NONE)
				return cmd;
		}
	}

	return DECODE_COMMAND_NONE;
}

enum decoder_command
decoder_tag(G_GNUC_UNUSED struct decoder *decoder, struct input_stream *is,
	    const struct tag *tag)
{
	enum decoder_command cmd;

	assert(dc.state == DECODE_STATE_DECODE);
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
		cmd = do_send_tag(is, merged);
		tag_free(merged);
	} else
		/* send only the decoder tag */
		cmd = do_send_tag(is, tag);

	return cmd;
}
