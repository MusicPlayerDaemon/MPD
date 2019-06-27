/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "util/Compiler.h"
#include "system/Error.hxx"

#ifdef _WIN32
#include <winsock2.h>
typedef DWORD socket_error_t;
#else
#include <errno.h>
typedef int socket_error_t;
#endif

gcc_pure
static inline socket_error_t
GetSocketError() noexcept
{
#ifdef _WIN32
	return WSAGetLastError();
#else
	return errno;
#endif
}

gcc_const
static inline bool
IsSocketErrorAgain(socket_error_t code) noexcept
{
#ifdef _WIN32
	return code == WSAEINPROGRESS;
#else
	return code == EAGAIN;
#endif
}

gcc_const
static inline bool
IsSocketErrorInterruped(socket_error_t code) noexcept
{
#ifdef _WIN32
	return code == WSAEINTR;
#else
	return code == EINTR;
#endif
}

gcc_const
static inline bool
IsSocketErrorClosed(socket_error_t code) noexcept
{
#ifdef _WIN32
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
#ifdef _WIN32
	char msg[256];
#else
	const char *const msg;
#endif

public:
	explicit SocketErrorMessage(socket_error_t code=GetSocketError()) noexcept;

	operator const char *() const {
		return msg;
	}
};

gcc_const
static inline std::system_error
MakeSocketError(socket_error_t code, const char *msg) noexcept
{
#ifdef _WIN32
	return MakeLastError(code, msg);
#else
	return MakeErrno(code, msg);
#endif
}

gcc_pure
static inline std::system_error
MakeSocketError(const char *msg) noexcept
{
	return MakeSocketError(GetSocketError(), msg);
}

#endif
