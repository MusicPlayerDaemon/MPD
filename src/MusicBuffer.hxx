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

#ifndef MPD_MUSIC_BUFFER_HXX
#define MPD_MUSIC_BUFFER_HXX

#include "MusicChunkPtr.hxx"
#include "util/SliceBuffer.hxx"
#include "thread/Mutex.hxx"

/**
 * An allocator for #MusicChunk objects.
 */
class MusicBuffer {
	/** a mutex which protects #buffer */
	mutable Mutex mutex;

	SliceBuffer<MusicChunk> buffer;

public:
	/**
	 * Creates a new #MusicBuffer object.
	 *
	 * @param num_chunks the number of #MusicChunk reserved in
	 * this buffer
	 */
	explicit MusicBuffer(unsigned num_chunks);

#ifndef NDEBUG
	/**
	 * Check whether the buffer is empty.  This call is not
	 * protected with the mutex, and may only be used while this
	 * object is inaccessible to other threads.
	 */
	bool IsEmptyUnsafe() const {
		return buffer.empty();
	}
#endif

	bool IsFull() const noexcept {
		const std::scoped_lock<Mutex> protect(mutex);
		return buffer.IsFull();
	}

	/**
	 * Returns the total number of reserved chunks in this buffer.  This
	 * is the same value which was passed to the constructor
	 * music_buffer_new().
	 */
	[[gnu::pure]]
	unsigned GetSize() const noexcept {
		return buffer.GetCapacity();
	}

	/**
	 * Allocates a chunk from the buffer.  When it is not used anymore,
	 * call Return().
	 *
	 * @return an empty chunk or nullptr if there are no chunks
	 * available
	 */
	MusicChunkPtr Allocate() noexcept;

	/**
	 * Returns a chunk to the buffer.  It can be reused by
	 * Allocate() then.
	 */
	void Return(MusicChunk *chunk) noexcept;
};

#endif
