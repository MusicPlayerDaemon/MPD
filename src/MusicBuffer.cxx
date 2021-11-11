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

#include "MusicBuffer.hxx"
#include "MusicChunk.hxx"

#include <cassert>

MusicBuffer::MusicBuffer(unsigned num_chunks)
	:buffer(num_chunks) {
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
