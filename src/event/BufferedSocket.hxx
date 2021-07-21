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

#ifndef MPD_BUFFERED_SOCKET_HXX
#define MPD_BUFFERED_SOCKET_HXX

#include "SocketEvent.hxx"
#include "util/StaticFifoBuffer.hxx"

#include <cassert>
#include <cstdint>
#include <exception>

class EventLoop;

/**
 * A #SocketEvent specialization that adds an input buffer.
 */
class BufferedSocket {
	StaticFifoBuffer<uint8_t, 8192> input;

protected:
	SocketEvent event;

public:
	using ssize_t = SocketEvent::ssize_t;

	BufferedSocket(SocketDescriptor _fd, EventLoop &_loop) noexcept
		:event(_loop, BIND_THIS_METHOD(OnSocketReady), _fd) {
		event.ScheduleRead();
	}

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	bool IsDefined() const noexcept {
		return event.IsDefined();
	}

	auto GetSocket() const noexcept {
		return event.GetSocket();
	}

	void Close() noexcept {
		event.Close();
	}

private:
	/**
	 * @return the number of bytes read from the socket, 0 if the
	 * socket isn't ready for reading, -1 on error (the socket has
	 * been closed and probably destructed)
	 */
	ssize_t DirectRead(void *data, size_t length) noexcept;

	/**
	 * Receive data from the socket to the input buffer.
	 *
	 * @return false if the socket has been closed
	 */
	bool ReadToBuffer() noexcept;

protected:
	/**
	 * @return false if the socket has been closed
	 */
	bool ResumeInput() noexcept;

	/**
	 * Mark a portion of the input buffer "consumed".  Only
	 * allowed to be called from OnSocketInput().  This method
	 * does not invalidate the pointer passed to OnSocketInput()
	 * yet.
	 */
	void ConsumeInput(size_t nbytes) noexcept {
		assert(IsDefined());

		input.Consume(nbytes);
	}

	enum class InputResult {
		/**
		 * The method was successful, and it is ready to
		 * read more data.
		 */
		MORE,

		/**
		 * The method does not want to get more data for now.
		 * It will call ResumeInput() when it's ready for
		 * more.
		 */
		PAUSE,

		/**
		 * The method wants to be called again immediately, if
		 * there's more data in the buffer.
		 */
		AGAIN,

		/**
		 * The method has closed the socket.
		 */
		CLOSED,
	};

	/**
	 * Data has been received on the socket.
	 *
	 * @param data a pointer to the beginning of the buffer; the
	 * buffer may be modified by the method while it processes the
	 * data
	 */
	virtual InputResult OnSocketInput(void *data, size_t length) noexcept = 0;

	virtual void OnSocketError(std::exception_ptr ep) noexcept = 0;
	virtual void OnSocketClosed() noexcept = 0;

	virtual void OnSocketReady(unsigned flags) noexcept;
};

#endif
