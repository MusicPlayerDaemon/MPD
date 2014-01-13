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

#ifndef MPD_SOCKET_ERROR_HXX
#define MPD_SOCKET_ERROR_HXX

#include "Compiler.h"
#include "util/Error.hxx" // IWYU pragma: export

#ifdef WIN32
#include <winsock2.h>
typedef DWORD socket_error_t;
#else
#include <errno.h>
typedef int socket_error_t;
#endif

class Domain;

/**
 * A #Domain for #Error for socket I/O errors.  The code is an errno
 * value (or WSAGetLastError() on Windows).
 */
extern const Domain socket_domain;

gcc_pure
static inline socket_error_t
GetSocketError()
{
#ifdef WIN32
	return WSAGetLastError();
#else
	return errno;
#endif
}

gcc_const
static inline bool
IsSocketErrorAgain(socket_error_t code)
{
#ifdef WIN32
	return code == WSAEINPROGRESS;
#else
	return code == EAGAIN;
#endif
}

gcc_const
static inline bool
IsSocketErrorInterruped(socket_error_t code)
{
#ifdef WIN32
	return code == WSAEINTR;
#else
	return code == EINTR;
#endif
}

gcc_const
static inline bool
IsSocketErrorClosed(socket_error_t code)
{
#ifdef WIN32
	return code == WSAECONNRESET;
#else
	return code == EPIPE || code == ECONNRESET;
#endif
}

/**
 * Helper class that formats a socket error message into a
 * human-readable string.  On Windows, a buffer is necessary for this,
 * and this class hosts the buffer.
 */
class SocketErrorMessage {
#ifdef WIN32
	char msg[256];
#else
	const char *const msg;
#endif

public:
#ifdef WIN32
	explicit SocketErrorMessage(socket_error_t code=GetSocketError());
#else
	explicit SocketErrorMessage(socket_error_t code=GetSocketError());
#endif

	operator const char *() const {
		return msg;
	}
};

static inline void
SetSocketError(Error &error, socket_error_t code)
{
	const SocketErrorMessage msg(code);
	error.Set(socket_domain, code, msg);
}

static inline void
SetSocketError(Error &error)
{
	SetSocketError(error, GetSocketError());
}

gcc_const
static inline Error
NewSocketError(socket_error_t code)
{
	Error error;
	SetSocketError(error, code);
	return error;
}

gcc_pure
static inline Error
NewSocketError()
{
	return NewSocketError(GetSocketError());
}

#endif
