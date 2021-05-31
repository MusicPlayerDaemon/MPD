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

#ifndef MPD_ENCODER_INTERFACE_HXX
#define MPD_ENCODER_INTERFACE_HXX

#include "EncoderPlugin.hxx"
#include "util/Compiler.h"

#include <cassert>
#include <cstddef>

struct AudioFormat;
struct Tag;

class Encoder {
	const bool implements_tag;

public:
	explicit Encoder(bool _implements_tag) noexcept
		:implements_tag(_implements_tag) {}
	virtual ~Encoder() noexcept = default;

	bool ImplementsTag() const noexcept {
		return implements_tag;
	}

	/**
	 * Ends the stream: flushes the encoder object, generate an
	 * end-of-stream marker (if applicable), make everything which
	 * might currently be buffered available by encoder_read().
	 *
	 * After this function has been called, the encoder may not be
	 * usable for more data, and only Read() and Close() can be
	 * called.
	 *
	 * Throws on error.
	 */
	virtual void End() {
	}

	/**
	 * Flushes an encoder object, make everything which might
	 * currently be buffered available by Read().
	 *
	 * Throws on error.
	 */
	virtual void Flush() {
	}

	/**
	 * Prepare for sending a tag to the encoder.  This is used by
	 * some encoders to flush the previous sub-stream, in
	 * preparation to begin a new one.
	 *
	 * Throws on error.
	 */
	virtual void PreTag() {
	}

	/**
	 * Sends a tag to the encoder.
	 *
	 * Instructions: call PreTag(); then obtain flushed data with
	 * Read(); finally call Tag().
	 *
	 * Throws on error.
	 *
	 * @param tag the tag object
	 */
	virtual void SendTag([[maybe_unused]] const Tag &tag) {
	}

	/**
	 * Writes raw PCM data to the encoder.
	 *
	 * Throws on error.
	 *
	 * @param data the buffer containing PCM samples
	 * @param length the length of the buffer in bytes
	 */
	virtual void Write(const void *data, size_t length) = 0;

	/**
	 * Reads encoded data from the encoder.
	 *
	 * Call this repeatedly until no more data is returned.
	 *
	 * @param dest the destination buffer to copy to
	 * @param length the maximum length of the destination buffer
	 * @return the number of bytes written to #dest
	 */
	virtual size_t Read(void *dest, size_t length) = 0;
};

class PreparedEncoder {
public:
	virtual ~PreparedEncoder() noexcept = default;

	/**
	 * Opens the object.  You must call this prior to using it.
	 * Before you free it, you must call Close().  You may open
	 * and close (reuse) one encoder any number of times.
	 *
	 * After this function returns successfully and before the
	 * first encoder_write() call, you should invoke
	 * encoder_read() to obtain the file header.
	 *
	 * Throws on error.
	 *
	 * @param audio_format the encoder's input audio format; the plugin
	 * may modify the struct to adapt it to its abilities
	 */
	virtual Encoder *Open(AudioFormat &audio_format) = 0;

	/**
	 * Get mime type of encoded content.
	 *
	 * @return an constant string, nullptr on failure
	 */
	virtual const char *GetMimeType() const noexcept {
		return nullptr;
	}
};

#endif
