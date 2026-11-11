// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <winbase.h> // for FormatMessage()

#ifdef _UNICODE
#include <stringapiset.h> // for WideCharToMultiByte()
#endif

#include <span>

template<std::size_t buffer_size>
inline std::size_t
FormatMessageUTF8(DWORD dwFlags, LPCVOID lpSource,
		  DWORD dwMessageId, DWORD wLanguageId,
		  std::span<char, buffer_size> buffer,
		  va_list *Arguments=nullptr) noexcept
{
#ifdef _UNICODE
	wchar_t wbuffer[buffer_size];
#else
	auto *wbuffer = buffer.data();
#endif

	DWORD nbytes = FormatMessage(dwFlags, lpSource, dwMessageId, wLanguageId,
				     wbuffer, buffer_size, Arguments);
	if (nbytes == 0)
		return 0;

#ifdef _UNICODE
	int length = WideCharToMultiByte(CP_UTF8, 0, wbuffer, nbytes,
					 buffer.data(), buffer.size() - 1,
					 nullptr, nullptr);
	if (length <= 0)
		return 0;

	buffer[length] = '\0';
	return static_cast<std::size_t>(length);
#else
	return nbytes;
#endif
}
