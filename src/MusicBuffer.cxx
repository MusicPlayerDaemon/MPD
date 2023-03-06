// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "MusicBuffer.hxx"
#include "MusicChunk.hxx"

#include <cassert>

MusicBuffer::MusicBuffer(unsigned num_chunks)
	:buffer(num_chunks)
{
	buffer.SetName("MusicBuffer");
}

MusicChunkPtr
MusicBuffer::Allocate() noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);
	return {buffer.Allocate(), MusicChunkDeleter(*this)};
}

void
MusicBuffer::Return(MusicChunk *chunk) noexcept
{
	assert(chunk != nullptr);

	/* these attributes need to be cleared before locking the
	   mutex, because they might recursively call this method,
	   causing a deadlock */
	chunk->next.reset();
	chunk->other.reset();

	const std::scoped_lock<Mutex> protect(mutex);

	assert(!chunk->other || !chunk->other->other);

	buffer.Free(chunk);
}
