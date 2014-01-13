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
#include "SocketError.hxx"
#include "util/Domain.hxx"

#include <string.h>

const Domain socket_domain("socket");

#ifdef WIN32

SocketErrorMessage::SocketErrorMessage(socket_error_t code)
{
	DWORD nbytes = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
				     FORMAT_MESSAGE_IGNORE_INSERTS |
				     FORMAT_MESSAGE_MAX_WIDTH_MASK,
				     NULL, code, 0,
				     (LPSTR)msg, sizeof(msg), NULL);
	if (nbytes == 0)
		strcpy(msg, "Unknown error");
}

#else

SocketErrorMessage::SocketErrorMessage(socket_error_t code)
	:msg(strerror(code)) {}

#endif
