// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "SharedPipeConsumer.hxx"
#include "MusicChunk.hxx"
#include "MusicPipe.hxx"

const MusicChunk *
SharedPipeConsumer::Get() noexcept
{
	if (chunk != nullptr) {
		if (!consumed)
			return chunk;

		if (chunk->next == nullptr)
			return nullptr;

		consumed = false;
		return chunk = chunk->next.get();
	} else {
		/* get the first chunk from the pipe */
		consumed = false;
		return chunk = pipe->Peek();
	}
}

bool
SharedPipeConsumer::IsConsumed(const MusicChunk &_chunk) const noexcept
{
	if (chunk == nullptr)
		return false;

	assert(&_chunk == chunk || pipe->Contains(chunk));

	if (&_chunk != chunk) {
		assert(_chunk.next != nullptr);
		return true;
	}

	return consumed && _chunk.next == nullptr;
}
