// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Win32.hxx"
#include "system/Error.hxx"
#include "util/AllocatedString.hxx"

#include <memory>

#include <windows.h>

AllocatedString
WideCharToMultiByte(unsigned code_page, std::wstring_view src)
{
	int length = WideCharToMultiByte(code_page, 0, src.data(), src.size(),
					 nullptr, 0,
					 nullptr, nullptr);
	if (length <= 0)
		throw MakeLastError("Failed to convert from Unicode");

	auto buffer = std::make_unique<char[]>(length + 1);
	length = WideCharToMultiByte(code_page, 0, src.data(), src.size(),
				     buffer.get(), length,
				     nullptr, nullptr);
	if (length <= 0)
		throw MakeLastError("Failed to convert from Unicode");

	buffer[length] = '\0';
	return AllocatedString::Donate(buffer.release());
}

BasicAllocatedString<wchar_t>
MultiByteToWideChar(unsigned code_page, std::string_view src)
{
	int length = MultiByteToWideChar(code_page, 0, src.data(), src.size(),
					 nullptr, 0);
	if (length <= 0)
		throw MakeLastError("Failed to convert to Unicode");

	auto buffer = std::make_unique<wchar_t[]>(length + 1);
	length = MultiByteToWideChar(code_page, 0, src.data(), src.size(),
				     buffer.get(), length);
	if (length <= 0)
		throw MakeLastError("Failed to convert to Unicode");

	buffer[length] = L'\0';
	return BasicAllocatedString<wchar_t>::Donate(buffer.release());
}
