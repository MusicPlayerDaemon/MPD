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

/*! \file
 * \brief The MPD Decoder API
 *
 * This is the public API which is used by decoder plugins to
 * communicate with the mpd core.
 */

#ifndef MPD_DECODER_API_H
#define MPD_DECODER_API_H

#include "check.h"
#include "decoder_command.h"
#include "decoder_plugin.h"
#include "input_stream.h"
#include "replay_gain_info.h"
#include "tag.h"
#include "audio_format.h"
#include "conf.h"

#include <stdbool.h>

/**
 * Notify the player thread that it has finished initialization and
 * that it has read the song's meta data.
 *
 * @param decoder the decoder object
 * @param audio_format the audio format which is going to be sent to
 * decoder_data()
 * @param seekable true if the song is seekable
 * @param total_time the total number of seconds in this song; -1 if unknown
 */
void
decoder_initialized(struct decoder *decoder,
		    const struct audio_format *audio_format,
		    bool seekable, float total_time);

/**
 * Determines the pending decoder command.
 *
 * @param decoder the decoder object
 * @return the current command, or DECODE_COMMAND_NONE if there is no
 * command pending
 */
enum decoder_command
decoder_get_command(struct decoder *decoder);

/**
 * Called by the decoder when it has performed the requested command
 * (dc->command).  This function resets dc->command and wakes up the
 * player thread.
 *
 * @param decoder the decoder object
 */
void
decoder_command_finished(struct decoder *decoder);

/**
 * Call this when you have received the DECODE_COMMAND_SEEK command.
 *
 * @param decoder the decoder object
 * @return the destination position for the week
 */
double
decoder_seek_where(struct decoder *decoder);

/**
 * Call this instead of decoder_command_finished() when seeking has
 * failed.
 *
 * @param decoder the decoder object
 */
void
decoder_seek_error(struct decoder *decoder);

/**
 * Blocking read from the input stream.
 *
 * @param decoder the decoder object
 * @param is the input stream to read from
 * @param buffer the destination buffer
 * @param length the maximum number of bytes to read
 * @return the number of bytes read, or 0 if one of the following
 * occurs: end of file; error; command (like SEEK or STOP).
 */
size_t
decoder_read(struct decoder *decoder, struct input_stream *is,
	     void *buffer, size_t length);

/**
 * Sets the time stamp for the next data chunk [seconds].  The MPD
 * core automatically counts it up, and a decoder plugin only needs to
 * use this function if it thinks that adding to the time stamp based
 * on the buffer size won't work.
 */
void
decoder_timestamp(struct decoder *decoder, double t);

/**
 * This function is called by the decoder plugin when it has
 * successfully decoded block of input data.
 *
 * @param decoder the decoder object
 * @param is an input stream which is buffering while we are waiting
 * for the player
 * @param data the source buffer
 * @param length the number of bytes in the buffer
 * @return the current command, or DECODE_COMMAND_NONE if there is no
 * command pending
 */
enum decoder_command
decoder_data(struct decoder *decoder, struct input_stream *is,
	     const void *data, size_t length,
	     uint16_t kbit_rate);

/**
 * This function is called by the decoder plugin when it has
 * successfully decoded a tag.
 *
 * @param decoder the decoder object
 * @param is an input stream which is buffering while we are waiting
 * for the player
 * @param tag the tag to send
 * @return the current command, or DECODE_COMMAND_NONE if there is no
 * command pending
 */
enum decoder_command
decoder_tag(struct decoder *decoder, struct input_stream *is,
	    const struct tag *tag);

/**
 * Set replay gain values for the following chunks.
 *
 * @param decoder the decoder object
 * @param rgi the replay_gain_info object; may be NULL to invalidate
 * the previous replay gain values
 * @return the replay gain adjustment used
 */
float
decoder_replay_gain(struct decoder *decoder,
		    const struct replay_gain_info *replay_gain_info);

/**
 * Store MixRamp tags.
 *
 * @param decoder the decoder object
 * @param replay_gain_db the ReplayGain adjustment used for this song
 * @param mixramp_start the mixramp_start tag; may be NULL to invalidate
 * @param mixramp_end the mixramp_end tag; may be NULL to invalidate
 */
void
decoder_mixramp(struct decoder *decoder, float replay_gain_db,
		char *mixramp_start, char *mixramp_end);

#endif
