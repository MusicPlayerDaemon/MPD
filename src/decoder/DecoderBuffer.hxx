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

#ifndef MPD_DECODER_BUFFER_HXX
#define MPD_DECODER_BUFFER_HXX

#include "util/DynamicFifoBuffer.hxx"
#include "util/ConstBuffer.hxx"

#include <cstddef>
#include <cstdint>

class DecoderClient;
class InputStream;

/**
 * This objects handles buffered reads in decoder plugins easily.  You
 * create a buffer object, and use its high-level methods to fill and
 * read it.  It will automatically handle shifting the buffer.
 */
class DecoderBuffer {
	DecoderClient *const client;
	InputStream &is;

	DynamicFifoBuffer<uint8_t> buffer;

public:
	/**
	 * Creates a new buffer.
	 *
	 * @param _client the decoder client, used for decoder_read(),
	 * may be nullptr
	 * @param _is the input stream object where we should read from
	 * @param _size the maximum size of the buffer
	 */
	DecoderBuffer(DecoderClient *_client, InputStream &_is,
		      size_t _size)
		:client(_client), is(_is), buffer(_size) {}

	const InputStream &GetStream() const noexcept {
		return is;
	}

	void Clear() noexcept {
		buffer.Clear();
	}

	/**
	 * Read data from the #InputStream and append it to the buffer.
	 *
	 * @return true if data was appended; false if there is no
	 * data available (yet), end of file, I/O error or a decoder
	 * command was received
	 */
	bool Fill();

	/**
	 * How many bytes are stored in the buffer?
	 */
	[[gnu::pure]]
	size_t GetAvailable() const noexcept {
		return buffer.GetAvailable();
	}

	/**
	 * Reads data from the buffer.  This data is not yet consumed,
	 * you have to call Consume() to do that.  The returned buffer
	 * becomes invalid after a Fill() or a Consume() call.
	 */
	ConstBuffer<void> Read() const noexcept {
		auto r = buffer.Read();
		return { r.data, r.size };
	}

	/**
	 * Wait until this number of bytes are available.  Returns nullptr on
	 * error.
	 */
	ConstBuffer<void> Need(size_t min_size);

	/**
	 * Consume (delete, invalidate) a part of the buffer.  The
	 * "nbytes" parameter must not be larger than the length
	 * returned by Read().
	 *
	 * @param nbytes the number of bytes to consume
	 */
	void Consume(size_t nbytes) noexcept {
		buffer.Consume(nbytes);
	}

	/**
	 * Skips the specified number of bytes, discarding its data.
	 *
	 * @param nbytes the number of bytes to skip
	 * @return true on success, false on error
	 */
	bool Skip(size_t nbytes);
};

#endif
