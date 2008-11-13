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
#include "gcc.h"

#include <assert.h>

void decoder_initialized(struct decoder * decoder,
			 const struct audio_format *audio_format,
			 bool seekable, float total_time)
{
	assert(dc.state == DECODE_STATE_START);
	assert(decoder != NULL);
	assert(!decoder->stream_tag_sent);
	assert(!decoder->seeking);
	assert(audio_format != NULL);
	assert(audio_format_defined(audio_format));

	pcm_convert_init(&decoder->conv_state);

	dc.in_audio_format = *audio_format;
	getOutputAudioFormat(audio_format, &dc.out_audio_format);

	dc.seekable = seekable;
	dc.total_time = total_time;

	dc.state = DECODE_STATE_DECODE;
	notify_signal(&pc.notify);
}

const char *decoder_get_url(mpd_unused struct decoder * decoder, char * buffer)
{
	return song_get_url(dc.current_song, buffer);
}

enum decoder_command decoder_get_command(mpd_unused struct decoder * decoder)
{
	return dc.command;
}

void decoder_command_finished(mpd_unused struct decoder * decoder)
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

double decoder_seek_where(mpd_unused struct decoder * decoder)
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
 * Add the tag items from the input stream (meta_name, meta_title) to
 * a duplicate of the specified tag.  The return value has to be freed
 * with tag_free().  If this function returns NULL, then there are no
 * tags provided by the stream.
 */
static struct tag *
tag_add_stream_tags(const struct tag *src_tag, const struct input_stream *is)
{
	struct tag *tag;

	assert(src_tag != NULL);
	assert(is != NULL);

	if ((is->meta_name == NULL || tag_has_type(src_tag, TAG_ITEM_NAME)) &&
	    (is->meta_title == NULL || tag_has_type(src_tag, TAG_ITEM_TITLE)))
	    return NULL;

	tag = tag_dup(src_tag);
	if (is->meta_name != NULL && !tag_has_type(src_tag, TAG_ITEM_NAME))
		tag_add_item(tag, TAG_ITEM_NAME, is->meta_name);
	if (is->meta_title != NULL && !tag_has_type(src_tag, TAG_ITEM_TITLE))
		tag_add_item(tag, TAG_ITEM_TITLE, is->meta_title);

	return tag;
}

/**
 * All chunks are full of decoded data; wait for the player to free
 * one.
 */
static enum decoder_command
need_chunks(struct input_stream *is)
{
	if (dc.command == DECODE_COMMAND_STOP ||
	    dc.command == DECODE_COMMAND_SEEK)
		return dc.command;

	if (is == NULL || input_stream_buffer(is) <= 0) {
		notify_wait(&dc.notify);
		notify_signal(&pc.notify);
	}

	return DECODE_COMMAND_NONE;
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

	if (is != NULL && !decoder->stream_tag_sent) {
		const struct tag *src;
		struct tag *tag1, *tag2;

		/* base is the current song's tag, or an empty new
		   tag if the song has no tag */
		src = dc.current_song->tag;
		if (src == NULL)
			src = tag1 = tag_new();
		else
			tag1 = NULL;

		tag2 = tag_add_stream_tags(src, is);
		if (tag1 != NULL)
			/* free the empty tag created by tag_new(), we
			   aren't going to send it */
			tag_free(tag1);

		if (tag2 != NULL)
			/* use the composite tag returned by
			   tag_add_stream_tags() */
			src = tag2;

		if (src != NULL) {
			music_pipe_tag(src);
			if (tag2 != NULL)
				tag_free(tag2);
		}

		decoder->stream_tag_sent = true;
	}

	if (audio_format_equals(&dc.in_audio_format, &dc.out_audio_format)) {
		data = _data;
	} else {
		length = pcm_convert_size(&dc.in_audio_format, length,
					  &dc.out_audio_format);
		if (length > conv_buffer_size) {
			if (conv_buffer != NULL)
				free(conv_buffer);
			conv_buffer = xmalloc(length);
			conv_buffer_size = length;
		}

		data = conv_buffer;
		length = pcm_convert(&dc.in_audio_format, _data,
				     length, &dc.out_audio_format,
				     data, &decoder->conv_state);
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
			enum decoder_command cmd = need_chunks(is);
			if (cmd != DECODE_COMMAND_NONE)
				return cmd;
		}
	}

	return DECODE_COMMAND_NONE;
}

enum decoder_command
decoder_tag(mpd_unused struct decoder *decoder, struct input_stream *is,
	    const struct tag *tag)
{
	struct tag *tag2 = is != NULL ? tag_add_stream_tags(tag, is) : NULL;

	assert(dc.state == DECODE_STATE_DECODE);

	if (tag2 != NULL)
		tag = tag2;

	while (!music_pipe_tag(tag)) {
		enum decoder_command cmd = need_chunks(is);
		if (cmd != DECODE_COMMAND_NONE)
			return cmd;
	}

	if (tag2 != NULL)
		tag_free(tag2);

	decoder->stream_tag_sent = true;

	return DECODE_COMMAND_NONE;
}
