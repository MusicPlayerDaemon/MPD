/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "Charset.hxx"
#include "Domain.hxx"
#include "Limits.hxx"
#include "Log.hxx"
#include "lib/icu/Converter.hxx"
#include "util/Error.hxx"

#ifdef WIN32
#include <windows.h>
#endif

#include <algorithm>

#include <assert.h>
#include <string.h>

#ifdef HAVE_FS_CHARSET

static std::string fs_charset;

static IcuConverter *fs_converter;

bool
SetFSCharset(const char *charset, Error &error)
{
	assert(charset != nullptr);
	assert(fs_converter == nullptr);

	fs_converter = IcuConverter::Create(charset, error);
	if (fs_converter == nullptr)
		return false;

	FormatDebug(path_domain,
		    "SetFSCharset: fs charset is: %s", fs_charset.c_str());
	return true;
}

#endif

void
DeinitFSCharset()
{
#ifdef HAVE_ICU_CONVERTER
	delete fs_converter;
	fs_converter = nullptr;
#endif
}

const char *
GetFSCharset()
{
#ifdef HAVE_FS_CHARSET
	return fs_charset.empty() ? "UTF-8" : fs_charset.c_str();
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
PathToUTF8(PathTraitsFS::const_pointer path_fs)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(path_fs != nullptr);
#endif

#ifdef WIN32
	int length = WideCharToMultiByte(CP_UTF8, 0, path_fs, -1, nullptr, 0,
					 nullptr, nullptr);
	if (length <= 0)
		return PathTraitsUTF8::string();

	char *buffer = new char[length];
	length = WideCharToMultiByte(CP_UTF8, 0, path_fs, -1, buffer, length,
				     nullptr, nullptr);
	if (length <= 0) {
		delete[] buffer;
		return PathTraitsUTF8::string();
	}

	PathTraitsUTF8::string result(buffer);
	delete[] buffer;
	return FixSeparators(std::move(result));
#else
#ifdef HAVE_FS_CHARSET
	if (fs_converter == nullptr)
#endif
		return FixSeparators(path_fs);
#ifdef HAVE_FS_CHARSET

	return FixSeparators(fs_converter->ToUTF8(path_fs));
#endif
#endif
}

#if defined(HAVE_FS_CHARSET) || defined(WIN32)

PathTraitsFS::string
PathFromUTF8(PathTraitsUTF8::const_pointer path_utf8)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(path_utf8 != nullptr);
#endif

#ifdef WIN32
	int length = MultiByteToWideChar(CP_UTF8, 0, path_utf8, -1,
					 nullptr, 0);
	if (length <= 0)
		return PathTraitsFS::string();

	wchar_t *buffer = new wchar_t[length];
	length = MultiByteToWideChar(CP_UTF8, 0, path_utf8, -1,
				     buffer, length);
	if (length <= 0) {
		delete[] buffer;
		return PathTraitsFS::string();
	}

	PathTraitsFS::string result(buffer);
	delete[] buffer;
	return std::move(result);
#else
	if (fs_converter == nullptr)
		return path_utf8;

	return fs_converter->FromUTF8(path_utf8);
#endif
}

#endif
