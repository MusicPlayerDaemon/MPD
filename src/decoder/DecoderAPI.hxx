/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "Client.hxx"
#include "input/Ptr.hxx"
#include "Command.hxx"
#include "DecoderPlugin.hxx"
#include "tag/ReplayGainInfo.hxx"
#include "tag/Tag.hxx"
#include "tag/MixRampInfo.hxx"
#include "pcm/AudioFormat.hxx"
#include "config/Block.hxx"
#include "Chrono.hxx"

// IWYU pragma: end_exports

#include <cstdint>

/**
 * Throw an instance of this class to stop decoding the current song
 * (successfully).  It can be used to jump out of all of a decoder's
 * stack frames.
 */
class StopDecoder {};

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
decoder_read(DecoderClient *decoder, InputStream &is,
	     void *buffer, size_t length) noexcept;

static inline size_t
decoder_read(DecoderClient &decoder, InputStream &is,
	     void *buffer, size_t length) noexcept
{
	return decoder_read(&decoder, is, buffer, length);
}

/**
 * Blocking read from the input stream.  Attempts to fill the buffer
 * as much as possible, until either end-of-file is reached or an
 * error occurs.
 *
 * @return the number of bytes read, or 0 if one of the following
 * occurs: end of file; error; command (like SEEK or STOP).
 */
size_t
decoder_read_much(DecoderClient *decoder, InputStream &is,
		  void *buffer, size_t size) noexcept;

/**
 * Blocking read from the input stream.  Attempts to fill the buffer
 * completely; there is no partial result.
 *
 * @return true on success, false on error or command or not enough
 * data
 */
bool
decoder_read_full(DecoderClient *decoder, InputStream &is,
		  void *buffer, size_t size) noexcept;

/**
 * Skip data on the #InputStream.
 *
 * @return true on success, false on error or command
 */
bool
decoder_skip(DecoderClient *decoder, InputStream &is, size_t size) noexcept;

#endif
