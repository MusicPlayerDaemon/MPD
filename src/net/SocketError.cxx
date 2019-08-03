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

#include "SocketError.hxx"

#include <iterator>

#include <string.h>

#ifdef _WIN32

SocketErrorMessage::SocketErrorMessage(socket_error_t code) noexcept
{
#ifdef _UNICODE
	wchar_t buffer[std::size(msg)];
#else
	auto *buffer = msg;
#endif

	DWORD nbytes = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
				     FORMAT_MESSAGE_IGNORE_INSERTS |
				     FORMAT_MESSAGE_MAX_WIDTH_MASK,
				     nullptr, code, 0,
				     buffer, std::size(msg), nullptr);
	if (nbytes == 0) {
		strcpy(msg, "Unknown error");
		return;
	}

#ifdef _UNICODE
	auto length = WideCharToMultiByte(CP_UTF8, 0, buffer, -1,
					  msg, std::size(msg),
					  nullptr, nullptr);
	if (length <= 0) {
		strcpy(msg, "WideCharToMultiByte() error");
		return;
	}
#endif
}

#else

SocketErrorMessage::SocketErrorMessage(socket_error_t code) noexcept
	:msg(strerror(code)) {}

#endif
