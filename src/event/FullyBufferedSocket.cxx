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
#include "FullyBufferedSocket.hxx"
#include "system/SocketError.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Compiler.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#endif

FullyBufferedSocket::ssize_t
FullyBufferedSocket::DirectWrite(const void *data, size_t length)
{
	const auto nbytes = SocketMonitor::Write((const char *)data, length);
	if (gcc_unlikely(nbytes < 0)) {
		const auto code = GetSocketError();
		if (IsSocketErrorAgain(code))
			return 0;

		IdleMonitor::Cancel();
		BufferedSocket::Cancel();

		if (IsSocketErrorClosed(code))
			OnSocketClosed();
		else
			OnSocketError(NewSocketError(code));
	}

	return nbytes;
}

bool
FullyBufferedSocket::Flush()
{
	assert(IsDefined());

	const auto data = output.Read();
	if (data.IsEmpty()) {
		IdleMonitor::Cancel();
		CancelWrite();
		return true;
	}

	auto nbytes = DirectWrite(data.data, data.size);
	if (gcc_unlikely(nbytes <= 0))
		return nbytes == 0;

	output.Consume(nbytes);

	if (output.IsEmpty()) {
		IdleMonitor::Cancel();
		CancelWrite();
	}

	return true;
}

bool
FullyBufferedSocket::Write(const void *data, size_t length)
{
	assert(IsDefined());

	if (length == 0)
		return true;

	const bool was_empty = output.IsEmpty();

	if (!output.Append(data, length)) {
		// TODO
		static constexpr Domain buffered_socket_domain("buffered_socket");
		Error error;
		error.Set(buffered_socket_domain, "Output buffer is full");
		OnSocketError(std::move(error));
		return false;
	}

	if (was_empty)
		IdleMonitor::Schedule();
	return true;
}

bool
FullyBufferedSocket::OnSocketReady(unsigned flags)
{
	if (flags & WRITE) {
		assert(!output.IsEmpty());
		assert(!IdleMonitor::IsActive());

		if (!Flush())
			return false;
	}

	if (!BufferedSocket::OnSocketReady(flags))
		return false;

	return true;
}

void
FullyBufferedSocket::OnIdle()
{
	if (Flush() && !output.IsEmpty())
		ScheduleWrite();
}
