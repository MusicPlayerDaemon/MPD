// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef SHARED_PIPE_CONSUMER_HXX
#define SHARED_PIPE_CONSUMER_HXX

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

	[[gnu::pure]]
	bool IsConsumed(const MusicChunk &_chunk) const noexcept;

	void ClearTail([[maybe_unused]] const MusicChunk &_chunk) noexcept {
		assert(chunk == &_chunk);
		assert(consumed);
		chunk = nullptr;
	}
};

#endif
