// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "SocketEvent.hxx"
#include "util/StaticFifoBuffer.hxx"

#include <cassert>
#include <cstddef>
#include <exception>
#include <span>
#include <type_traits>

class EventLoop;

/**
 * A #SocketEvent specialization that adds an input buffer.
 */
class BufferedSocket {
	StaticFifoBuffer<std::byte, 8192> input;

protected:
	SocketEvent event;

public:
	using ssize_t = std::make_signed<size_t>::type;

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
	ssize_t DirectRead(std::span<std::byte> dest) noexcept;

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
