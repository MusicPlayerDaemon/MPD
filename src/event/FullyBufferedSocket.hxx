// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "BufferedSocket.hxx"
#include "IdleEvent.hxx"
#include "util/PeakBuffer.hxx"

#include <span>

/**
 * A #BufferedSocket specialization that adds an output buffer.
 */
class FullyBufferedSocket : protected BufferedSocket {
	IdleEvent idle_event;

	PeakBuffer output;

public:
	FullyBufferedSocket(SocketDescriptor _fd, EventLoop &_loop,
			    size_t normal_size, size_t peak_size=0) noexcept
		:BufferedSocket(_fd, _loop),
		 idle_event(_loop, BIND_THIS_METHOD(OnIdle)),
		 output(normal_size, peak_size) {
	}

	using BufferedSocket::GetEventLoop;
	using BufferedSocket::IsDefined;

	void Close() noexcept {
		idle_event.Cancel();
		BufferedSocket::Close();
	}

	std::size_t GetOutputMaxSize() const noexcept {
		return output.max_size();
	}

private:
	/**
	 * @return the number of bytes written to the socket, 0 if the
	 * socket isn't ready for writing, -1 on error (the socket has
	 * been closed and probably destructed)
	 */
	ssize_t DirectWrite(std::span<const std::byte> src) noexcept;

protected:
	/**
	 * Send data from the output buffer to the socket.
	 *
	 * @return false if the socket has been closed
	 */
	bool Flush() noexcept;

	/**
	 * @return false if the socket has been closed
	 */
	bool Write(const void *data, size_t length) noexcept;

	void OnIdle() noexcept;

	/* virtual methods from class BufferedSocket */
	void OnSocketReady(unsigned flags) noexcept override;
};
