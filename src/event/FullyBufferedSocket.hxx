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

#ifndef MPD_FULLY_BUFFERED_SOCKET_HXX
#define MPD_FULLY_BUFFERED_SOCKET_HXX

#include "BufferedSocket.hxx"
#include "IdleEvent.hxx"
#include "util/PeakBuffer.hxx"

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
	ssize_t DirectWrite(const void *data, size_t length) noexcept;

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

#endif
