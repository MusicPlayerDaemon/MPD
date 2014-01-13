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

#ifndef MPD_FULLY_BUFFERED_SOCKET_HXX
#define MPD_FULLY_BUFFERED_SOCKET_HXX

#include "check.h"
#include "BufferedSocket.hxx"
#include "IdleMonitor.hxx"
#include "util/PeakBuffer.hxx"

/**
 * A #BufferedSocket specialization that adds an output buffer.
 */
class FullyBufferedSocket : protected BufferedSocket, private IdleMonitor {
	PeakBuffer output;

public:
	FullyBufferedSocket(int _fd, EventLoop &_loop,
			    size_t normal_size, size_t peak_size=0)
		:BufferedSocket(_fd, _loop), IdleMonitor(_loop),
		 output(normal_size, peak_size) {
	}

	using BufferedSocket::IsDefined;

	void Close() {
		IdleMonitor::Cancel();
		BufferedSocket::Close();
	}

private:
	ssize_t DirectWrite(const void *data, size_t length);

protected:
	/**
	 * Send data from the output buffer to the socket.
	 *
	 * @return false if the socket has been closed
	 */
	bool Flush();

	/**
	 * @return false if the socket has been closed
	 */
	bool Write(const void *data, size_t length);

	virtual bool OnSocketReady(unsigned flags) override;
	virtual void OnIdle() override;
};

#endif
