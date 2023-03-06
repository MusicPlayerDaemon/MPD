// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ICU_WIN32_HXX
#define MPD_ICU_WIN32_HXX

#include <string_view>

class AllocatedString;
template<typename T> class BasicAllocatedString;

/**
 * Throws std::system_error on error.
 */
[[gnu::pure]]
AllocatedString
WideCharToMultiByte(unsigned code_page, std::wstring_view src);

/**
 * Throws std::system_error on error.
 */
[[gnu::pure]]
BasicAllocatedString<wchar_t>
MultiByteToWideChar(unsigned code_page, std::string_view src);

#endif
