// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "MusicChunkPtr.hxx"
#include "MusicBuffer.hxx"

void
MusicChunkDeleter::operator()(MusicChunk *chunk) noexcept
{
	buffer->Return(chunk);
}
