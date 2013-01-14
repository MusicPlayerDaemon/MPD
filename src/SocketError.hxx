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

#ifndef MPD_SOCKET_ERROR_HXX
#define MPD_SOCKET_ERROR_HXX

#include "gcc.h"

#include <glib.h>

#ifdef WIN32
#include <winsock2.h>
typedef DWORD socket_error_t;
#else
#include <errno.h>
typedef int socket_error_t;
#endif

/**
 * A GQuark for GError for socket I/O errors.  The code is an errno
 * value (or WSAGetLastError() on Windows).
 */
gcc_const
static inline GQuark
SocketErrorQuark(void)
{
	return g_quark_from_static_string("socket");
}

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

static inline void
SetSocketError(GError **error_r, socket_error_t code)
{
#ifdef WIN32
	if (error_r == NULL)
		return;

	char buffer[256];
	DWORD nbytes = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
				     FORMAT_MESSAGE_IGNORE_INSERTS |
				     FORMAT_MESSAGE_MAX_WIDTH_MASK,
				     NULL, code, 0,
				     (LPSTR)buffer, sizeof(buffer), NULL);
	const char *msg = nbytes > 0
		? buffer
		: "Unknown error";
	g_set_error_literal(error_r, SocketErrorQuark(), code, msg);
#else
	g_set_error_literal(error_r, SocketErrorQuark(), code,
			    g_strerror(code));
#endif
}

static inline void
SetSocketError(GError **error_r)
{
	SetSocketError(error_r, GetSocketError());
}

gcc_malloc
static inline GError *
NewSocketError(socket_error_t code)
{
#ifdef WIN32
	char buffer[256];
	DWORD nbytes = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
				     FORMAT_MESSAGE_IGNORE_INSERTS |
				     FORMAT_MESSAGE_MAX_WIDTH_MASK,
				     NULL, code, 0,
				     (LPSTR)buffer, sizeof(buffer), NULL);
	const char *msg = nbytes > 0
		? buffer
		: "Unknown error";
	return g_error_new_literal(SocketErrorQuark(), code, msg);
#else
	return g_error_new_literal(SocketErrorQuark(), code, g_strerror(code));
#endif
}

gcc_malloc
static inline GError *
NewSocketError()
{
	return NewSocketError(GetSocketError());
}

#endif
