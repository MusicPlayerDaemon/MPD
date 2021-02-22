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
