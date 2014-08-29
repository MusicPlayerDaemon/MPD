/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_DECODER_API_HXX
#define MPD_DECODER_API_HXX

// IWYU pragma: begin_exports

#include "check.h"
#include "DecoderCommand.hxx"
#include "DecoderPlugin.hxx"
#include "ReplayGainInfo.hxx"
#include "tag/Tag.hxx"
#include "AudioFormat.hxx"
#include "MixRampInfo.hxx"
#include "config/ConfigData.hxx"
#include "Chrono.hxx"

// IWYU pragma: end_exports

#include <stdint.h>

class Error;

/**
 * Notify the player thread that it has finished initialization and
 * that it has read the song's meta data.
 *
 * @param decoder the decoder object
 * @param audio_format the audio format which is going to be sent to
 * decoder_data()
 * @param seekable true if the song is seekable
 * @param duration the total duration of this song; negative if
 * unknown
 */
void
decoder_initialized(Decoder &decoder,
		    AudioFormat audio_format,
		    bool seekable, SignedSongTime duration);

/**
 * Determines the pending decoder command.
 *
 * @param decoder the decoder object
 * @return the current command, or DecoderCommand::NONE if there is no
 * command pending
 */
gcc_pure
DecoderCommand
decoder_get_command(Decoder &decoder);

/**
 * Called by the decoder when it has performed the requested command
 * (dc->command).  This function resets dc->command and wakes up the
 * player thread.
 *
 * @param decoder the decoder object
 */
void
decoder_command_finished(Decoder &decoder);

/**
 * Call this when you have received the DecoderCommand::SEEK command.
 *
 * @param decoder the decoder object
 * @return the destination position for the seek in milliseconds
 */
gcc_pure
SongTime
decoder_seek_time(Decoder &decoder);

/**
 * Call this when you have received the DecoderCommand::SEEK command.
 *
 * @param decoder the decoder object
 * @return the destination position for the seek in frames
 */
gcc_pure
uint64_t
decoder_seek_where_frame(Decoder &decoder);

/**
 * Call this instead of decoder_command_finished() when seeking has
 * failed.
 *
 * @param decoder the decoder object
 */
void
decoder_seek_error(Decoder &decoder);

/**
 * Open a new #InputStream and wait until it's ready.  Can get
 * cancelled by DecoderCommand::STOP (returns nullptr without setting
 * #Error).
 */
InputStream *
decoder_open_uri(Decoder &decoder, const char *uri, Error &error);

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
decoder_read(Decoder *decoder, InputStream &is,
	     void *buffer, size_t length);

static inline size_t
decoder_read(Decoder &decoder, InputStream &is,
	     void *buffer, size_t length)
{
	return decoder_read(&decoder, is, buffer, length);
}

/**
 * Blocking read from the input stream.  Attempts to fill the buffer
 * completely; there is no partial result.
 *
 * @return true on success, false on error or command or not enough
 * data
 */
bool
decoder_read_full(Decoder *decoder, InputStream &is,
		  void *buffer, size_t size);

/**
 * Skip data on the #InputStream.
 *
 * @return true on success, false on error or command
 */
bool
decoder_skip(Decoder *decoder, InputStream &is, size_t size);

/**
 * Sets the time stamp for the next data chunk [seconds].  The MPD
 * core automatically counts it up, and a decoder plugin only needs to
 * use this function if it thinks that adding to the time stamp based
 * on the buffer size won't work.
 */
void
decoder_timestamp(Decoder &decoder, double t);

/**
 * This function is called by the decoder plugin when it has
 * successfully decoded block of input data.
 *
 * @param decoder the decoder object
 * @param is an input stream which is buffering while we are waiting
 * for the player
 * @param data the source buffer
 * @param length the number of bytes in the buffer
 * @return the current command, or DecoderCommand::NONE if there is no
 * command pending
 */
DecoderCommand
decoder_data(Decoder &decoder, InputStream *is,
	     const void *data, size_t length,
	     uint16_t kbit_rate);

static inline DecoderCommand
decoder_data(Decoder &decoder, InputStream &is,
	     const void *data, size_t length,
	     uint16_t kbit_rate)
{
	return decoder_data(decoder, &is, data, length, kbit_rate);
}

/**
 * This function is called by the decoder plugin when it has
 * successfully decoded a tag.
 *
 * @param decoder the decoder object
 * @param is an input stream which is buffering while we are waiting
 * for the player
 * @param tag the tag to send
 * @return the current command, or DecoderCommand::NONE if there is no
 * command pending
 */
DecoderCommand
decoder_tag(Decoder &decoder, InputStream *is, Tag &&tag);

static inline DecoderCommand
decoder_tag(Decoder &decoder, InputStream &is, Tag &&tag)
{
	return decoder_tag(decoder, &is, std::move(tag));
}

/**
 * Set replay gain values for the following chunks.
 *
 * @param decoder the decoder object
 * @param rgi the replay_gain_info object; may be nullptr to invalidate
 * the previous replay gain values
 */
void
decoder_replay_gain(Decoder &decoder,
		    const ReplayGainInfo *replay_gain_info);

/**
 * Store MixRamp tags.
 *
 * @param decoder the decoder object
 * @param mixramp_start the mixramp_start tag; may be nullptr to invalidate
 * @param mixramp_end the mixramp_end tag; may be nullptr to invalidate
 */
void
decoder_mixramp(Decoder &decoder, MixRampInfo &&mix_ramp);

#endif
