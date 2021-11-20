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

#ifndef MPD_PIPE_H
#define MPD_PIPE_H

#include "MusicChunkPtr.hxx"
#include "thread/Mutex.hxx"

#ifndef NDEBUG
#include "pcm/AudioFormat.hxx"
#endif

/**
 * A queue of #MusicChunk objects.  One party appends chunks at the
 * tail, and the other consumes them from the head.
 */
class MusicPipe {
	/** the first chunk */
	MusicChunkPtr head;

	/** a pointer to the tail of the chunk */
	MusicChunkPtr *tail_r = &head;

	/** the current number of chunks */
	unsigned size = 0;

	/** a mutex which protects #head and #tail_r */
	mutable Mutex mutex;

#ifndef NDEBUG
	AudioFormat audio_format = AudioFormat::Undefined();
#endif

public:
	~MusicPipe() noexcept {
		Clear();
	}

#ifndef NDEBUG
	/**
	 * Checks if the audio format if the chunk is equal to the specified
	 * audio_format.
	 */
	[[gnu::pure]]
	bool CheckFormat(AudioFormat other) const noexcept {
		return !audio_format.IsDefined() ||
			audio_format == other;
	}

	/**
	 * Checks if the specified chunk is enqueued in the music pipe.
	 */
	[[gnu::pure]]
	bool Contains(const MusicChunk *chunk) const noexcept;
#endif

	/**
	 * Returns the first #MusicChunk from the pipe.  Returns
	 * nullptr if the pipe is empty.
	 */
	[[gnu::pure]]
	const MusicChunk *Peek() const noexcept {
		const std::scoped_lock<Mutex> protect(mutex);
		return head.get();
	}

	/**
	 * Removes the first chunk from the head, and returns it.
	 */
	MusicChunkPtr Shift() noexcept;

	/**
	 * Clears the whole pipe and returns the chunks to the buffer.
	 */
	void Clear() noexcept;

	/**
	 * Pushes a chunk to the tail of the pipe.
	 */
	void Push(MusicChunkPtr chunk) noexcept;

	/**
	 * Returns the number of chunks currently in this pipe.
	 */
	[[gnu::pure]]
	unsigned GetSize() const noexcept {
		const std::scoped_lock<Mutex> protect(mutex);
		return size;
	}

	[[gnu::pure]]
	bool IsEmpty() const noexcept {
		return GetSize() == 0;
	}
};

#endif
