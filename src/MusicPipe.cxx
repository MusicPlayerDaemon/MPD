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

#include "MusicPipe.hxx"
#include "MusicChunk.hxx"

#include <cassert>

#ifndef NDEBUG

bool
MusicPipe::Contains(const MusicChunk *chunk) const noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);

	for (const MusicChunk *i = head.get(); i != nullptr; i = i->next.get())
		if (i == chunk)
			return true;

	return false;
}

#endif

MusicChunkPtr
MusicPipe::Shift() noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);

	auto chunk = std::move(head);
	if (chunk != nullptr) {
		assert(!chunk->IsEmpty());

		head = std::move(chunk->next);
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
		if (size == 0)
			audio_format.Clear();
#endif
	}

	return chunk;
}

void
MusicPipe::Clear() noexcept
{
	while (Shift()) {}
}

void
MusicPipe::Push(MusicChunkPtr chunk) noexcept
{
	assert(!chunk->IsEmpty());
	assert(chunk->length == 0 || chunk->audio_format.IsValid());

	const std::scoped_lock<Mutex> protect(mutex);

	assert(size > 0 || !audio_format.IsDefined());
	assert(!audio_format.IsDefined() ||
	       chunk->CheckFormat(audio_format));

#ifndef NDEBUG
	if (!audio_format.IsDefined() && chunk->length > 0)
		audio_format = chunk->audio_format;
#endif

	chunk->next.reset();
	*tail_r = std::move(chunk);
	tail_r = &(*tail_r)->next;

	++size;
}
