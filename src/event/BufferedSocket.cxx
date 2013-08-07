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

#include "config.h"
#include "BufferedSocket.hxx"
#include "system/SocketError.hxx"
#include "util/fifo_buffer.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

BufferedSocket::~BufferedSocket()
{
	if (input != nullptr)
		fifo_buffer_free(input);
}

BufferedSocket::ssize_t
BufferedSocket::DirectRead(void *data, size_t length)
{
	const auto nbytes = SocketMonitor::Read((char *)data, length);
	if (gcc_likely(nbytes > 0))
		return nbytes;

	if (nbytes == 0) {
		OnSocketClosed();
		return -1;
	}

	const auto code = GetSocketError();
	if (IsSocketErrorAgain(code))
		return 0;

	if (IsSocketErrorClosed(code))
		OnSocketClosed();
	else
		OnSocketError(NewSocketError(code));
	return -1;
}

bool
BufferedSocket::ReadToBuffer()
{
	assert(IsDefined());

	if (input == nullptr)
		input = fifo_buffer_new(8192);

	size_t length;
	void *buffer = fifo_buffer_write(input, &length);
	assert(buffer != nullptr);

	const auto nbytes = DirectRead(buffer, length);
	if (nbytes > 0)
		fifo_buffer_append(input, nbytes);

	return nbytes >= 0;
}

bool
BufferedSocket::ResumeInput()
{
	assert(IsDefined());

	if (input == nullptr) {
		ScheduleRead();
		return true;
	}

	while (true) {
		size_t length;
		const void *data = fifo_buffer_read(input, &length);
		if (data == nullptr) {
			ScheduleRead();
			return true;
		}

		const auto result = OnSocketInput(data, length);
		switch (result) {
		case InputResult::MORE:
			if (fifo_buffer_is_full(input)) {
				// TODO
				OnSocketError(g_error_new_literal(g_quark_from_static_string("buffered_socket"),
								  0, "Input buffer is full"));
				return false;
			}

			ScheduleRead();
			return true;

		case InputResult::PAUSE:
			CancelRead();
			return true;

		case InputResult::AGAIN:
			continue;

		case InputResult::CLOSED:
			return false;
		}
	}
}

void
BufferedSocket::ConsumeInput(size_t nbytes)
{
	assert(IsDefined());

	fifo_buffer_consume(input, nbytes);
}

bool
BufferedSocket::OnSocketReady(unsigned flags)
{
	assert(IsDefined());

	if (gcc_unlikely(flags & (ERROR|HANGUP))) {
		OnSocketClosed();
		return false;
	}

	if (flags & READ) {
		assert(input == nullptr || !fifo_buffer_is_full(input));

		if (!ReadToBuffer() || !ResumeInput())
			return false;

		if (input == nullptr || !fifo_buffer_is_full(input))
			ScheduleRead();
	}

	return true;
}
