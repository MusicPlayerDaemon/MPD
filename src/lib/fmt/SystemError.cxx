/*
 * Copyright 2022 Max Kellermann <max.kellermann@gmail.com>
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

#include "SystemError.hxx"
#include "ToBuffer.hxx"

#include <array>

std::system_error
VFmtSystemError(std::error_code code,
		fmt::string_view format_str, fmt::format_args args) noexcept
{
	const auto msg = VFmtBuffer<512>(format_str, args);
	return std::system_error{code, msg};
}

#ifdef _WIN32

#include <windef.h> // for HWND (needed by winbase.h)
#include <winbase.h> // for FormatMessageA()

std::system_error
VFmtLastError(DWORD code,
	      fmt::string_view format_str, fmt::format_args args) noexcept
{
	std::array<char, 512> buffer;
	const auto end = buffer.data() + buffer.size();

	constexpr std::size_t max_prefix = sizeof(buffer) - 128;
	auto [p, _] = fmt::vformat_to_n(buffer.data(),
					buffer.size() - max_prefix,
					format_str, args);
	*p++ = ':';
	*p++ = ' ';

	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
		       FORMAT_MESSAGE_IGNORE_INSERTS,
		       nullptr, code, 0, p, end - p, nullptr);

	return MakeLastError(code, buffer.data());
}

#endif // _WIN32
