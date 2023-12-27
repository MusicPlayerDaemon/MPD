// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Charset.hxx"
#include "Features.hxx"
#include "Domain.hxx"
#include "lib/icu/Converter.hxx"
#include "util/AllocatedString.hxx"
#include "config.h"

#ifdef _WIN32
#include "lib/icu/Win32.hxx"
#include <windows.h>
#endif

#include <algorithm>
#include <cassert>

#ifdef HAVE_FS_CHARSET

static std::string fs_charset;

static std::unique_ptr<IcuConverter> fs_converter;

void
SetFSCharset(const char *charset)
{
	assert(charset != nullptr);
	assert(fs_converter == nullptr);

	fs_charset = charset;
	fs_converter = IcuConverter::Create(charset);
	assert(fs_converter != nullptr);
}

#endif

void
DeinitFSCharset() noexcept
{
#ifdef HAVE_FS_CHARSET
	fs_converter.reset();
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

static inline PathTraitsUTF8::string
FixSeparators(const PathTraitsUTF8::string_view _s)
{
	// For whatever reason GCC can't convert constexpr to value reference.
	// This leads to link errors when passing separators directly.
	auto to = PathTraitsUTF8::SEPARATOR;
	decltype(to) from = PathTraitsFS::SEPARATOR;

	PathTraitsUTF8::string s(_s);

	if (from != to)
		/* convert backslash to slash on WIN32 */
		std::replace(s.begin(), s.end(), from, to);

	return s;
}

PathTraitsUTF8::string
PathToUTF8(PathTraitsFS::string_view path_fs)
{
#ifdef _WIN32
	const auto buffer = WideCharToMultiByte(CP_UTF8, path_fs);
	return FixSeparators(buffer);
#else
#ifdef HAVE_FS_CHARSET
	if (fs_converter == nullptr)
#endif
		return FixSeparators(path_fs);
#ifdef HAVE_FS_CHARSET

	const auto buffer = fs_converter->ToUTF8(path_fs);
	return FixSeparators(buffer);
#endif
#endif
}

#if defined(HAVE_FS_CHARSET) || defined(_WIN32)

PathTraitsFS::string
PathFromUTF8(PathTraitsUTF8::string_view path_utf8)
{
#ifdef _WIN32
	const auto buffer = MultiByteToWideChar(CP_UTF8, path_utf8);
	return PathTraitsFS::string(buffer);
#else
	if (fs_converter == nullptr)
		return PathTraitsFS::string(path_utf8);

	const auto buffer = fs_converter->FromUTF8(path_utf8);
	return PathTraitsFS::string(buffer);
#endif
}

#endif
