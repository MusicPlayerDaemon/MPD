/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "check.h"
#include "SocketMonitor.hxx"
#include "util/StaticFifoBuffer.hxx"

#include <assert.h>
#include <stdint.h>

class Error;
class EventLoop;

/**
 * A #SocketMonitor specialization that adds an input buffer.
 */
class BufferedSocket : protected SocketMonitor {
	StaticFifoBuffer<uint8_t, 8192> input;

public:
	BufferedSocket(int _fd, EventLoop &_loop)
		:SocketMonitor(_fd, _loop) {
		ScheduleRead();
	}

	using SocketMonitor::IsDefined;
	using SocketMonitor::Close;
	using SocketMonitor::Write;

private:
	ssize_t DirectRead(void *data, size_t length);

	/**
	 * Receive data from the socket to the input buffer.
	 *
	 * @return false if the socket has been closed
	 */
	bool ReadToBuffer();

protected:
	/**
	 * @return false if the socket has been closed
	 */
	bool ResumeInput();

	/**
	 * Mark a portion of the input buffer "consumed".  Only
	 * allowed to be called from OnSocketInput().  This method
	 * does not invalidate the pointer passed to OnSocketInput()
	 * yet.
	 */
	void ConsumeInput(size_t nbytes) {
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
	virtual InputResult OnSocketInput(void *data, size_t length) = 0;

	virtual void OnSocketError(Error &&error) = 0;
	virtual void OnSocketClosed() = 0;

	virtual bool OnSocketReady(unsigned flags) override;
};

#endif
