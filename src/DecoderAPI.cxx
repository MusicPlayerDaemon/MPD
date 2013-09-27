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
#include "DecoderAPI.hxx"
#include "AudioConfig.hxx"
#include "replay_gain_config.h"
#include "MusicChunk.hxx"
#include "MusicBuffer.hxx"
#include "MusicPipe.hxx"
#include "DecoderControl.hxx"
#include "DecoderInternal.hxx"
#include "Song.hxx"
#include "InputStream.hxx"
#include "util/Error.hxx"

#include <glib.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "decoder"

void
decoder_initialized(struct decoder *decoder,
		    const AudioFormat audio_format,
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
	assert(audio_format.IsDefined());
	assert(audio_format.IsValid());

	dc->in_audio_format = audio_format;
	dc->out_audio_format = getOutputAudioFormat(audio_format);

	dc->seekable = seekable;
	dc->total_time = total_time;

	dc->Lock();
	dc->state = DECODE_STATE_DECODE;
	dc->client_cond.signal();
	dc->Unlock();

	g_debug("audio_format=%s, seekable=%s",
		audio_format_to_string(dc->in_audio_format, &af_string),
		seekable ? "true" : "false");

	if (dc->in_audio_format != dc->out_audio_format)
		g_debug("converting to %s",
			audio_format_to_string(dc->out_audio_format,
					       &af_string));
}

/**
 * Checks if we need an "initial seek".  If so, then the initial seek
 * is prepared, and the function returns true.
 */
gcc_pure
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

		if (dc->command == DecoderCommand::NONE) {
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
gcc_pure
static DecoderCommand
decoder_get_virtual_command(struct decoder *decoder)
{
	const struct decoder_control *dc = decoder->dc;
	assert(dc->pipe != NULL);

	if (decoder_prepare_initial_seek(decoder))
		return DecoderCommand::SEEK;

	return dc->command;
}

DecoderCommand
decoder_get_command(struct decoder *decoder)
{
	return decoder_get_virtual_command(decoder);
}

void
decoder_command_finished(struct decoder *decoder)
{
	struct decoder_control *dc = decoder->dc;

	dc->Lock();

	assert(dc->command != DecoderCommand::NONE ||
	       decoder->initial_seek_running);
	assert(dc->command != DecoderCommand::SEEK ||
	       decoder->initial_seek_running ||
	       dc->seek_error || decoder->seeking);
	assert(dc->pipe != NULL);

	if (decoder->initial_seek_running) {
		assert(!decoder->seeking);
		assert(decoder->chunk == NULL);
		assert(dc->pipe->IsEmpty());

		decoder->initial_seek_running = false;
		decoder->timestamp = dc->start_ms / 1000.;
		dc->Unlock();
		return;
	}

	if (decoder->seeking) {
		decoder->seeking = false;

		/* delete frames from the old song position */

		if (decoder->chunk != NULL) {
			dc->buffer->Return(decoder->chunk);
			decoder->chunk = NULL;
		}

		dc->pipe->Clear(*dc->buffer);

		decoder->timestamp = dc->seek_where;
	}

	dc->command = DecoderCommand::NONE;
	dc->client_cond.signal();
	dc->Unlock();
}

double decoder_seek_where(gcc_unused struct decoder * decoder)
{
	const struct decoder_control *dc = decoder->dc;

	assert(dc->pipe != NULL);

	if (decoder->initial_seek_running)
		return dc->start_ms / 1000.;

	assert(dc->command == DecoderCommand::SEEK);

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

	assert(dc->command == DecoderCommand::SEEK);

	dc->seek_error = true;
	decoder->seeking = false;

	decoder_command_finished(decoder);
}

/**
 * Should be read operation be cancelled?  That is the case when the
 * player thread has sent a command such as "STOP".
 */
gcc_pure
static inline bool
decoder_check_cancel_read(const struct decoder *decoder)
{
	if (decoder == NULL)
		return false;

	const struct decoder_control *dc = decoder->dc;
	if (dc->command == DecoderCommand::NONE)
		return false;

	/* ignore the SEEK command during initialization, the plugin
	   should handle that after it has initialized successfully */
	if (dc->command == DecoderCommand::SEEK &&
	    (dc->state == DECODE_STATE_START || decoder->seeking))
		return false;

	return true;
}

size_t decoder_read(struct decoder *decoder,
		    struct input_stream *is,
		    void *buffer, size_t length)
{
	/* XXX don't allow decoder==NULL */

	assert(decoder == NULL ||
	       decoder->dc->state == DECODE_STATE_START ||
	       decoder->dc->state == DECODE_STATE_DECODE);
	assert(is != NULL);
	assert(buffer != NULL);

	if (length == 0)
		return 0;

	is->Lock();

	while (true) {
		if (decoder_check_cancel_read(decoder)) {
			is->Unlock();
			return 0;
		}

		if (is->IsAvailable())
			break;

		is->cond.wait(is->mutex);
	}

	Error error;
	size_t nbytes = is->Read(buffer, length, error);
	assert(nbytes == 0 || !error.IsDefined());
	assert(nbytes > 0 || error.IsDefined() || is->IsEOF());

	if (gcc_unlikely(nbytes == 0 && error.IsDefined()))
		g_warning("%s", error.GetMessage());

	is->Unlock();

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
static DecoderCommand
do_send_tag(struct decoder *decoder, const Tag &tag)
{
	struct music_chunk *chunk;

	if (decoder->chunk != NULL) {
		/* there is a partial chunk - flush it, we want the
		   tag in a new chunk */
		decoder_flush_chunk(decoder);
		decoder->dc->client_cond.signal();
	}

	assert(decoder->chunk == NULL);

	chunk = decoder_get_chunk(decoder);
	if (chunk == NULL) {
		assert(decoder->dc->command != DecoderCommand::NONE);
		return decoder->dc->command;
	}

	chunk->tag = new Tag(tag);
	return DecoderCommand::NONE;
}

static bool
update_stream_tag(struct decoder *decoder, struct input_stream *is)
{
	Tag *tag;

	tag = is != NULL
		? is->LockReadTag()
		: NULL;
	if (tag == NULL) {
		tag = decoder->song_tag;
		if (tag == NULL)
			return false;

		/* no stream tag present - submit the song tag
		   instead */
		decoder->song_tag = NULL;
	}

	delete decoder->stream_tag;
	decoder->stream_tag = tag;
	return true;
}

DecoderCommand
decoder_data(struct decoder *decoder,
	     struct input_stream *is,
	     const void *data, size_t length,
	     uint16_t kbit_rate)
{
	struct decoder_control *dc = decoder->dc;
	DecoderCommand cmd;

	assert(dc->state == DECODE_STATE_DECODE);
	assert(dc->pipe != NULL);
	assert(length % dc->in_audio_format.GetFrameSize() == 0);

	dc->Lock();
	cmd = decoder_get_virtual_command(decoder);
	dc->Unlock();

	if (cmd == DecoderCommand::STOP || cmd == DecoderCommand::SEEK ||
	    length == 0)
		return cmd;

	/* send stream tags */

	if (update_stream_tag(decoder, is)) {
		if (decoder->decoder_tag != NULL) {
			/* merge with tag from decoder plugin */
			Tag *tag = Tag::Merge(*decoder->decoder_tag,
					      *decoder->stream_tag);
			cmd = do_send_tag(decoder, *tag);
			delete tag;
		} else
			/* send only the stream tag */
			cmd = do_send_tag(decoder, *decoder->stream_tag);

		if (cmd != DecoderCommand::NONE)
			return cmd;
	}

	if (dc->in_audio_format != dc->out_audio_format) {
		Error error;
		data = decoder->conv_state.Convert(dc->in_audio_format,
						   data, length,
						   dc->out_audio_format,
						   &length,
						   error);
		if (data == NULL) {
			/* the PCM conversion has failed - stop
			   playback, since we have no better way to
			   bail out */
			g_warning("%s", error.GetMessage());
			return DecoderCommand::STOP;
		}
	}

	while (length > 0) {
		struct music_chunk *chunk;
		size_t nbytes;
		bool full;

		chunk = decoder_get_chunk(decoder);
		if (chunk == NULL) {
			assert(dc->command != DecoderCommand::NONE);
			return dc->command;
		}

		void *dest = chunk->Write(dc->out_audio_format,
					  decoder->timestamp -
					  dc->song->start_ms / 1000.0,
					  kbit_rate, &nbytes);
		if (dest == NULL) {
			/* the chunk is full, flush it */
			decoder_flush_chunk(decoder);
			dc->client_cond.signal();
			continue;
		}

		assert(nbytes > 0);

		if (nbytes > length)
			nbytes = length;

		/* copy the buffer */

		memcpy(dest, data, nbytes);

		/* expand the music pipe chunk */

		full = chunk->Expand(dc->out_audio_format, nbytes);
		if (full) {
			/* the chunk is full, flush it */
			decoder_flush_chunk(decoder);
			dc->client_cond.signal();
		}

		data = (const uint8_t *)data + nbytes;
		length -= nbytes;

		decoder->timestamp += (double)nbytes /
			dc->out_audio_format.GetTimeToSize();

		if (dc->end_ms > 0 &&
		    decoder->timestamp >= dc->end_ms / 1000.0)
			/* the end of this range has been reached:
			   stop decoding */
			return DecoderCommand::STOP;
	}

	return DecoderCommand::NONE;
}

DecoderCommand
decoder_tag(gcc_unused struct decoder *decoder, struct input_stream *is,
	    Tag &&tag)
{
	gcc_unused const struct decoder_control *dc = decoder->dc;
	DecoderCommand cmd;

	assert(dc->state == DECODE_STATE_DECODE);
	assert(dc->pipe != NULL);

	/* save the tag */

	delete decoder->decoder_tag;
	decoder->decoder_tag = new Tag(tag);

	/* check for a new stream tag */

	update_stream_tag(decoder, is);

	/* check if we're seeking */

	if (decoder_prepare_initial_seek(decoder))
		/* during initial seek, no music chunk must be created
		   until seeking is finished; skip the rest of the
		   function here */
		return DecoderCommand::SEEK;

	/* send tag to music pipe */

	if (decoder->stream_tag != NULL) {
		/* merge with tag from input stream */
		Tag *merged;

		merged = Tag::Merge(*decoder->stream_tag,
				    *decoder->decoder_tag);
		cmd = do_send_tag(decoder, *merged);
		delete merged;
	} else
		/* send only the decoder tag */
		cmd = do_send_tag(decoder, *decoder->decoder_tag);

	return cmd;
}

void
decoder_replay_gain(struct decoder *decoder,
		    const struct replay_gain_info *replay_gain_info)
{
	assert(decoder != NULL);

	if (replay_gain_info != NULL) {
		static unsigned serial;
		if (++serial == 0)
			serial = 1;

		if (REPLAY_GAIN_OFF != replay_gain_mode) {
			enum replay_gain_mode rgm = replay_gain_mode;
			if (rgm != REPLAY_GAIN_ALBUM)
				rgm = REPLAY_GAIN_TRACK;

			decoder->dc->replay_gain_db = 20.0 * log10f(
				replay_gain_tuple_scale(
					&replay_gain_info->tuples[rgm],
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
			decoder->dc->client_cond.signal();
		}
	} else
		decoder->replay_gain_serial = 0;
}

void
decoder_mixramp(struct decoder *decoder,
		char *mixramp_start, char *mixramp_end)
{
	assert(decoder != NULL);
	struct decoder_control *dc = decoder->dc;
	assert(dc != NULL);

	dc->MixRampStart(mixramp_start);
	dc->MixRampEnd(mixramp_end);
}
