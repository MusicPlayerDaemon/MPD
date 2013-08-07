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
#include "FullyBufferedSocket.hxx"
#include "system/SocketError.hxx"
#include "util/fifo_buffer.h"

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

		Cancel();

		if (IsSocketErrorClosed(code))
			OnSocketClosed();
		else
			OnSocketError(NewSocketError(code));
	}

	return nbytes;
}

bool
FullyBufferedSocket::WriteFromBuffer()
{
	assert(IsDefined());

	size_t length;
	const void *data = output.Read(&length);
	if (data == nullptr) {
		CancelWrite();
		return true;
	}

	auto nbytes = DirectWrite(data, length);
	if (gcc_unlikely(nbytes <= 0))
		return nbytes == 0;

	output.Consume(nbytes);

	if (output.IsEmpty())
		CancelWrite();

	return true;
}

bool
FullyBufferedSocket::Write(const void *data, size_t length)
{
	assert(IsDefined());

#if 0
	/* TODO: disabled because this would add overhead on some callers (the ones that often), but it may be useful */

	if (output.IsEmpty()) {
		/* try to write it directly first */
		const auto nbytes = DirectWrite(data, length);
		if (gcc_likely(nbytes > 0)) {
			data = (const uint8_t *)data + nbytes;
			length -= nbytes;
			if (length == 0)
				return true;
		} else if (nbytes < 0)
			return false;
	}
#endif

	if (!output.Append(data, length)) {
		// TODO
		OnSocketError(g_error_new_literal(g_quark_from_static_string("buffered_socket"),
						  0, "Output buffer is full"));
		return false;
	}

	ScheduleWrite();
	return true;
}

bool
FullyBufferedSocket::OnSocketReady(unsigned flags)
{
	const bool was_empty = output.IsEmpty();
	if (!BufferedSocket::OnSocketReady(flags))
		return false;

	if (was_empty && !output.IsEmpty())
		/* just in case the OnSocketInput() method has added
		   data to the output buffer: try to send it now
		   instead of waiting for the next event loop
		   iteration */
		flags |= WRITE;

	if (flags & WRITE) {
		assert(!output.IsEmpty());

		if (!WriteFromBuffer())
			return false;
	}

	return true;
}
