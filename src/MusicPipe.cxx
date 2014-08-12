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
#include "MusicPipe.hxx"
#include "MusicBuffer.hxx"
#include "MusicChunk.hxx"

#ifndef NDEBUG

bool
MusicPipe::Contains(const MusicChunk *chunk) const
{
	const ScopeLock protect(mutex);

	for (const MusicChunk *i = head; i != nullptr; i = i->next)
		if (i == chunk)
			return true;

	return false;
}

#endif

MusicChunk *
MusicPipe::Shift()
{
	const ScopeLock protect(mutex);

	MusicChunk *chunk = head;
	if (chunk != nullptr) {
		assert(!chunk->IsEmpty());

		head = chunk->next;
		--size;

		if (head == nullptr) {
			assert(size == 0);
			assert(tail_r == &chunk->next);

			tail_r = &head;
		} else {
			assert(size > 0);
			assert(tail_r != &chunk->next);
		}

#ifndef NDEBUG
		/* poison the "next" reference */
		chunk->next = (MusicChunk *)(void *)0x01010101;

		if (size == 0)
			audio_format.Clear();
#endif
	}

	return chunk;
}

void
MusicPipe::Clear(MusicBuffer &buffer)
{
	MusicChunk *chunk;

	while ((chunk = Shift()) != nullptr)
		buffer.Return(chunk);
}

void
MusicPipe::Push(MusicChunk *chunk)
{
	assert(!chunk->IsEmpty());
	assert(chunk->length == 0 || chunk->audio_format.IsValid());

	const ScopeLock protect(mutex);

	assert(size > 0 || !audio_format.IsDefined());
	assert(!audio_format.IsDefined() ||
	       chunk->CheckFormat(audio_format));

#ifndef NDEBUG
	if (!audio_format.IsDefined() && chunk->length > 0)
		audio_format = chunk->audio_format;
#endif

	chunk->next = nullptr;
	*tail_r = chunk;
	tail_r = &chunk->next;

	++size;
}
