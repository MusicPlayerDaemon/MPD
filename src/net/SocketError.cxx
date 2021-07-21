/*
 * Copyright 2015-2021 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "SocketError.hxx"

#include <iterator>

#include <string.h>

#ifdef _WIN32

SocketErrorMessage::SocketErrorMessage(socket_error_t code) noexcept
{
#ifdef _UNICODE
	wchar_t buffer[msg_size];
#else
	auto *buffer = msg;
#endif

	DWORD nbytes = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
				     FORMAT_MESSAGE_IGNORE_INSERTS |
				     FORMAT_MESSAGE_MAX_WIDTH_MASK,
				     nullptr, code, 0,
				     buffer, msg_size, nullptr);
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
