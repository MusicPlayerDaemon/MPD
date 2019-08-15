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

#include "Charset.hxx"
#include "Features.hxx"
#include "Domain.hxx"
#include "Log.hxx"
#include "lib/icu/Converter.hxx"
#include "util/AllocatedString.hxx"
#include "config.h"

#ifdef _WIN32
#include "lib/icu/Win32.hxx"
#include <windows.h>
#endif

#include <algorithm>

#include <assert.h>

#ifdef HAVE_FS_CHARSET

static std::string fs_charset;

static IcuConverter *fs_converter;

void
SetFSCharset(const char *charset)
{
	assert(charset != nullptr);
	assert(fs_converter == nullptr);

	fs_converter = IcuConverter::Create(charset);
	assert(fs_converter != nullptr);

	FormatDebug(path_domain,
		    "SetFSCharset: fs charset is: %s", fs_charset.c_str());
}

#endif

void
DeinitFSCharset() noexcept
{
#ifdef HAVE_ICU_CONVERTER
	delete fs_converter;
	fs_converter = nullptr;
#endif
}

const char *
GetFSCharset() noexcept
{
#ifdef HAVE_FS_CHARSET
	return fs_charset.empty() ? "UTF-8" : fs_charset.c_str();
#elif defined(_WIN32)
	return "ACP";
#else
	return "UTF-8";
#endif
}

static inline PathTraitsUTF8::string &&
FixSeparators(PathTraitsUTF8::string &&s)
{
	// For whatever reason GCC can't convert constexpr to value reference.
	// This leads to link errors when passing separators directly.
	auto to = PathTraitsUTF8::SEPARATOR;
	decltype(to) from = PathTraitsFS::SEPARATOR;

	if (from != to)
		/* convert backslash to slash on WIN32 */
		std::replace(s.begin(), s.end(), from, to);

	return std::move(s);
}

PathTraitsUTF8::string
PathToUTF8(PathTraitsFS::const_pointer_type path_fs)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(path_fs != nullptr);
#endif

#ifdef _WIN32
	const auto buffer = WideCharToMultiByte(CP_UTF8, path_fs);
	return FixSeparators(PathTraitsUTF8::string(buffer.c_str()));
#else
#ifdef HAVE_FS_CHARSET
	if (fs_converter == nullptr)
#endif
		return FixSeparators(path_fs);
#ifdef HAVE_FS_CHARSET

	const auto buffer = fs_converter->ToUTF8(path_fs);
	return FixSeparators(PathTraitsUTF8::string(buffer.c_str()));
#endif
#endif
}

#if defined(HAVE_FS_CHARSET) || defined(_WIN32)

PathTraitsFS::string
PathFromUTF8(PathTraitsUTF8::const_pointer_type path_utf8)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(path_utf8 != nullptr);
#endif

#ifdef _WIN32
	const auto buffer = MultiByteToWideChar(CP_UTF8, path_utf8);
	return PathTraitsFS::string(buffer.c_str());
#else
	if (fs_converter == nullptr)
		return path_utf8;

	const auto buffer = fs_converter->FromUTF8(path_utf8);
	return PathTraitsFS::string(buffer.c_str());
#endif
}

#endif
