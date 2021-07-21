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

#ifndef MPD_MUSIC_CHUNK_PTR_HXX
#define MPD_MUSIC_CHUNK_PTR_HXX

#include <memory>

struct MusicChunk;
class MusicBuffer;

class MusicChunkDeleter {
	MusicBuffer *buffer;

public:
	MusicChunkDeleter() = default;
	explicit MusicChunkDeleter(MusicBuffer &_buffer):buffer(&_buffer) {}

	void operator()(MusicChunk *chunk) noexcept;
};

using MusicChunkPtr = std::unique_ptr<MusicChunk, MusicChunkDeleter>;

#endif
