// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
