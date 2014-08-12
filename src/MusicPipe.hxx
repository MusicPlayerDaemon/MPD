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

#ifndef MPD_PIPE_H
#define MPD_PIPE_H

#include "thread/Mutex.hxx"
#include "Compiler.h"

#ifndef NDEBUG
#include "AudioFormat.hxx"
#endif

#include <assert.h>

struct MusicChunk;
class MusicBuffer;

/**
 * A queue of #MusicChunk objects.  One party appends chunks at the
 * tail, and the other consumes them from the head.
 */
class MusicPipe {
	/** the first chunk */
	MusicChunk *head;

	/** a pointer to the tail of the chunk */
	MusicChunk **tail_r;

	/** the current number of chunks */
	unsigned size;

	/** a mutex which protects #head and #tail_r */
	mutable Mutex mutex;

#ifndef NDEBUG
	AudioFormat audio_format;
#endif

public:
	/**
	 * Creates a new #MusicPipe object.  It is empty.
	 */
	MusicPipe()
		:head(nullptr), tail_r(&head), size(0) {
#ifndef NDEBUG
		audio_format.Clear();
#endif
	}

	/**
	 * Frees the object.  It must be empty now.
	 */
	~MusicPipe() {
		assert(head == nullptr);
		assert(tail_r == &head);
	}

#ifndef NDEBUG
	/**
	 * Checks if the audio format if the chunk is equal to the specified
	 * audio_format.
	 */
	gcc_pure
	bool CheckFormat(AudioFormat other) const {
		return !audio_format.IsDefined() ||
			audio_format == other;
	}

	/**
	 * Checks if the specified chunk is enqueued in the music pipe.
	 */
	gcc_pure
	bool Contains(const MusicChunk *chunk) const;
#endif

	/**
	 * Returns the first #MusicChunk from the pipe.  Returns
	 * nullptr if the pipe is empty.
	 */
	gcc_pure
	const MusicChunk *Peek() const {
		return head;
	}

	/**
	 * Removes the first chunk from the head, and returns it.
	 */
	MusicChunk *Shift();

	/**
	 * Clears the whole pipe and returns the chunks to the buffer.
	 *
	 * @param buffer the buffer object to return the chunks to
	 */
	void Clear(MusicBuffer &buffer);

	/**
	 * Pushes a chunk to the tail of the pipe.
	 */
	void Push(MusicChunk *chunk);

	/**
	 * Returns the number of chunks currently in this pipe.
	 */
	gcc_pure
	unsigned GetSize() const {
		return size;
	}

	gcc_pure
	bool IsEmpty() const {
		return GetSize() == 0;
	}
};

#endif
