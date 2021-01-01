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
