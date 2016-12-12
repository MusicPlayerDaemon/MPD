/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#include "check.h"
#include "Compiler.h"

#include <assert.h>

struct MusicChunk;
class MusicPipe;

/**
 * A utility class which helps with consuming data from a #MusicPipe.
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
	 * Has the output finished playing #current_chunk?
	 */
	bool consumed;

public:
	void Init(const MusicPipe &_pipe) {
		pipe = &_pipe;
		chunk = nullptr;
	}

	void Deinit() {
		pipe = nullptr;
		chunk = nullptr;
	}

	const MusicPipe &GetPipe() {
		assert(pipe != nullptr);

		return *pipe;
	}

	bool IsInitial() {
		return chunk == nullptr;
	}

	void Cancel() {
		chunk = nullptr;
	}

	const MusicChunk *Get();

	void Consume(gcc_unused const MusicChunk &_chunk) {
		assert(chunk != nullptr);
		assert(chunk == &_chunk);

		consumed = true;
	}

	gcc_pure
	bool IsConsumed(const MusicChunk &_chunk) const;

	void ClearTail(gcc_unused const MusicChunk &_chunk) {
		assert(chunk == &_chunk);
		assert(consumed);
		chunk = nullptr;
	}
};

#endif
