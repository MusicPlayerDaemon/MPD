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

#include "config.h"
#include "MusicBuffer.hxx"
#include "MusicChunk.hxx"
#include "system/FatalError.hxx"

#include <assert.h>

MusicBuffer::MusicBuffer(unsigned num_chunks)
	:buffer(num_chunks) {
	if (buffer.IsOOM())
		FatalError("Failed to allocate buffer");
}

MusicChunk *
MusicBuffer::Allocate()
{
	const ScopeLock protect(mutex);
	return buffer.Allocate();
}

void
MusicBuffer::Return(MusicChunk *chunk)
{
	assert(chunk != nullptr);

	const ScopeLock protect(mutex);

	if (chunk->other != nullptr) {
		assert(chunk->other->other == nullptr);
		buffer.Free(chunk->other);
	}

	buffer.Free(chunk);
}
