/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "util/PeakBuffer.hxx"
#include "gcc.h"

struct fifo_buffer;
class EventLoop;

class BufferedSocket : private SocketMonitor {
	fifo_buffer *input;
	PeakBuffer output;

public:
	BufferedSocket(int _fd, EventLoop &_loop,
		       size_t normal_size, size_t peak_size=0)
		:SocketMonitor(_fd, _loop), input(nullptr),
		 output(normal_size, peak_size) {
		ScheduleRead();
	}

	~BufferedSocket();

	using SocketMonitor::IsDefined;
	using SocketMonitor::Close;

private:
	ssize_t DirectWrite(const void *data, size_t length);
	ssize_t DirectRead(void *data, size_t length);

	/**
	 * Send data from the output buffer to the socket.
	 *
	 * @return false if the socket has been closed
	 */
	bool WriteFromBuffer();

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
	bool Write(const void *data, size_t length);

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
	void ConsumeInput(size_t nbytes);

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

	virtual InputResult OnSocketInput(const void *data, size_t length) = 0;
	virtual void OnSocketError(GError *error) = 0;
	virtual void OnSocketClosed() = 0;

private:
	virtual void OnSocketReady(unsigned flags) override;
};

#endif
