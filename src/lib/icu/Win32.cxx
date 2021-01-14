/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
