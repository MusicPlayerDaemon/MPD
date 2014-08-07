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

#include "config.h"
#include "BufferedSocket.hxx"
#include "system/SocketError.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Compiler.h"

#include <algorithm>

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

	const auto buffer = input.Write();
	assert(!buffer.IsEmpty());

	const auto nbytes = DirectRead(buffer.data, buffer.size);
	if (nbytes > 0)
		input.Append(nbytes);

	return nbytes >= 0;
}

bool
BufferedSocket::ResumeInput()
{
	assert(IsDefined());

	while (true) {
		const auto buffer = input.Read();
		if (buffer.IsEmpty()) {
			ScheduleRead();
			return true;
		}

		const auto result = OnSocketInput(buffer.data, buffer.size);
		switch (result) {
		case InputResult::MORE:
			if (input.IsFull()) {
				// TODO
				static constexpr Domain buffered_socket_domain("buffered_socket");
				Error error;
				error.Set(buffered_socket_domain,
					  "Input buffer is full");
				OnSocketError(std::move(error));
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

bool
BufferedSocket::OnSocketReady(unsigned flags)
{
	assert(IsDefined());

	if (gcc_unlikely(flags & (ERROR|HANGUP))) {
		OnSocketClosed();
		return false;
	}

	if (flags & READ) {
		assert(!input.IsFull());

		if (!ReadToBuffer() || !ResumeInput())
			return false;

		if (!input.IsFull())
			ScheduleRead();
	}

	return true;
}
