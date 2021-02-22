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

#ifndef MPD_OUTPUT_SNAPCAST_CLIENT_HXX
#define MPD_OUTPUT_SNAPCAST_CLIENT_HXX

#include "Chunk.hxx"
#include "event/BufferedSocket.hxx"
#include "util/IntrusiveList.hxx"

#include <chrono>
#include <cstdint>

struct SnapcastBase;
struct SnapcastTime;
class SnapcastOutput;
class UniqueSocketDescriptor;

class SnapcastClient final : BufferedSocket, public IntrusiveListHook
{
	SnapcastOutput &output;

	/**
	 * A queue of #Page objects to be sent to the client.
	 */
	SnapcastChunkQueue chunks;

	uint16_t next_id = 1;

	bool active = false;

public:
	SnapcastClient(SnapcastOutput &output,
		       UniqueSocketDescriptor _fd) noexcept;

	~SnapcastClient() noexcept;

	/**
	 * Frees the client and removes it from the server's client list.
	 *
	 * Caller must lock the mutex.
	 */
	void Close() noexcept;

	void LockClose() noexcept;

	void SendStreamTags(ConstBuffer<void> payload) noexcept;

	/**
	 * Caller must lock the mutex.
	 */
	void Push(SnapcastChunkPtr chunk) noexcept;

	/**
	 * Caller must lock the mutex.
	 */
	bool IsDrained() const noexcept {
		return chunks.empty();
	}

	/**
	 * Caller must lock the mutex.
	 */
	void Cancel() noexcept {
		ClearQueue(chunks);
	}

private:
	SnapcastChunkPtr LockPopQueue() noexcept;

	bool SendWireChunk(ConstBuffer<void> payload,
			   std::chrono::steady_clock::time_point t) noexcept;

	bool SendServerSettings(const SnapcastBase &request) noexcept;
	bool SendCodecHeader(const SnapcastBase &request) noexcept;
	bool SendTime(const SnapcastBase &request_header,
		      const SnapcastTime &request_payload) noexcept;

	/* virtual methods from class BufferedSocket */
	void OnSocketReady(unsigned flags) noexcept override;
	InputResult OnSocketInput(void *data, size_t length) noexcept override;
	void OnSocketError(std::exception_ptr ep) noexcept override;
	void OnSocketClosed() noexcept override;
};

#endif
