// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SNAPCAST_CHUNK_HXX
#define MPD_SNAPCAST_CHUNK_HXX

#include "util/AllocatedArray.hxx"

#include <chrono>
#include <cstddef>
#include <list>
#include <memory>
#include <queue>

/**
 * A chunk of data to be transmitted to connected Snapcast clients.
 */
struct SnapcastChunk {
	std::chrono::steady_clock::time_point time;
	AllocatedArray<std::byte> payload;

	SnapcastChunk(std::chrono::steady_clock::time_point _time,
		      AllocatedArray<std::byte> &&_payload) noexcept
		:time(_time), payload(std::move(_payload)) {}
};

using SnapcastChunkPtr = std::shared_ptr<SnapcastChunk>;

using SnapcastChunkQueue = std::queue<SnapcastChunkPtr,
				      std::list<SnapcastChunkPtr>>;

inline void
ClearQueue(SnapcastChunkQueue &q) noexcept
{
	while (!q.empty())
		q.pop();
}

#endif
