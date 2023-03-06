// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OUTPUT_SNAPCAST_CLIENT_HXX
#define MPD_OUTPUT_SNAPCAST_CLIENT_HXX

#include "Chunk.hxx"
#include "event/BufferedSocket.hxx"
#include "util/IntrusiveList.hxx"

#include <chrono>
#include <cstdint>
#include <span>

struct SnapcastBase;
struct SnapcastTime;
class SnapcastOutput;
class UniqueSocketDescriptor;

class SnapcastClient final : BufferedSocket, public IntrusiveListHook<>
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

	void SendStreamTags(std::span<const std::byte> payload) noexcept;

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

	bool SendWireChunk(std::span<const std::byte> payload,
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
