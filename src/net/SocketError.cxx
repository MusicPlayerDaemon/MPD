// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SocketError.hxx"

#include <iterator>

#include <string.h>

#ifdef _WIN32

#include "system/windows/FormatMessage.hxx" // for FormatMessageUTF8()

SocketErrorMessage::SocketErrorMessage(socket_error_t code) noexcept
{
	std::size_t length = FormatMessageUTF8(FORMAT_MESSAGE_FROM_SYSTEM |
					       FORMAT_MESSAGE_IGNORE_INSERTS |
					       FORMAT_MESSAGE_MAX_WIDTH_MASK,
					       nullptr, code, 0,
					       std::span{msg});
	if (length == 0)
		strcpy(msg, "Unknown error");
}

#else

SocketErrorMessage::SocketErrorMessage(socket_error_t code) noexcept
	:msg(strerror(code)) {}

#endif
