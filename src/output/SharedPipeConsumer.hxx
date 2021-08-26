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

#ifndef SHARED_PIPE_CONSUMER_HXX
#define SHARED_PIPE_CONSUMER_HXX

#include "util/Compiler.h"

#include <cassert>

struct MusicChunk;
class MusicPipe;

/**
 * A utility class which helps with consuming data from a #MusicPipe.
 *
 * This class is intentionally not thread-safe.  Since it is designed
 * to be called from two distinct threads (PlayerThread=feeder and
 * OutputThread=consumer), all methods must be called with a mutex
 * locked to serialize access.  Usually, this is #AudioOutput::mutex.
 */
class SharedPipeConsumer {
	/**
	 * The music pipe which provides music chunks to be played.
	 */
	const MusicPipe *pipe = nullptr;

	/**
	 * The #MusicChunk which is currently being played.  All
	 * chunks before this one may be returned to the #MusicBuffer,
	 * because they are not going to be used by this output
	 * anymore.
	 */
	const MusicChunk *chunk;

	/**
	 * Has the output finished playing #chunk?
	 */
	bool consumed;

public:
	void Init(const MusicPipe &_pipe) {
		pipe = &_pipe;
		chunk = nullptr;
	}

	const MusicPipe &GetPipe() {
		assert(pipe != nullptr);

		return *pipe;
	}

	bool IsInitial() const {
		return chunk == nullptr;
	}

	void Cancel() {
		chunk = nullptr;
	}

	const MusicChunk *Get() noexcept;

	void Consume([[maybe_unused]] const MusicChunk &_chunk) {
		assert(chunk != nullptr);
		assert(chunk == &_chunk);

		consumed = true;
	}

	gcc_pure
	bool IsConsumed(const MusicChunk &_chunk) const noexcept;

	void ClearTail([[maybe_unused]] const MusicChunk &_chunk) noexcept {
		assert(chunk == &_chunk);
		assert(consumed);
		chunk = nullptr;
	}
};

#endif
