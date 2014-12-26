/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "system/FatalError.hxx"
#include "Log.hxx"
#include "Traits.hxx"

#ifdef HAVE_GLIB
#include <glib.h>
#endif

#include <algorithm>

#include <assert.h>
#include <string.h>

#ifdef HAVE_GLIB

/**
 * Maximal number of bytes required to represent path name in UTF-8
 * (including nul-terminator).
 * This value is a rought estimate of upper bound.
 * It's based on path name limit in bytes (MPD_PATH_MAX)
 * and assumption that some weird encoding could represent some UTF-8 4 byte
 * sequences with single byte.
 */
static constexpr size_t MPD_PATH_MAX_UTF8 = (MPD_PATH_MAX - 1) * 4 + 1;

static std::string fs_charset;

gcc_pure
static bool
IsSupportedCharset(const char *charset)
{
	/* convert a space to check if the charset is valid */
	char *test = g_convert(" ", 1, charset, "UTF-8", nullptr, nullptr, nullptr);
	if (test == nullptr)
		return false;

	g_free(test);
	return true;
}

void
SetFSCharset(const char *charset)
{
	assert(charset != nullptr);

	if (!IsSupportedCharset(charset))
		FormatFatalError("invalid filesystem charset: %s", charset);

	fs_charset = charset;

	FormatDebug(path_domain,
		    "SetFSCharset: fs charset is: %s", fs_charset.c_str());
}

#endif

const char *
GetFSCharset()
{
#ifdef HAVE_GLIB
	return fs_charset.empty() ? "UTF-8" : fs_charset.c_str();
#else
	return "UTF-8";
#endif
}

static inline void FixSeparators(std::string &s)
{
#ifdef WIN32
	// For whatever reason GCC can't convert constexpr to value reference.
	// This leads to link errors when passing separators directly.
	auto from = PathTraitsFS::SEPARATOR;
	auto to = PathTraitsUTF8::SEPARATOR;
	std::replace(s.begin(), s.end(), from, to);
#else
	(void)s;
#endif
}

std::string
PathToUTF8(const char *path_fs)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(path_fs != nullptr);
#endif

#ifdef HAVE_GLIB
	if (fs_charset.empty()) {
#endif
		auto result = std::string(path_fs);
		FixSeparators(result);
		return result;
#ifdef HAVE_GLIB
	}

	GIConv conv = g_iconv_open("utf-8", fs_charset.c_str());
	if (conv == reinterpret_cast<GIConv>(-1))
		return std::string();

	// g_iconv() does not need nul-terminator,
	// std::string could be created without it too.
	char path_utf8[MPD_PATH_MAX_UTF8 - 1];
	char *in = const_cast<char *>(path_fs);
	char *out = path_utf8;
	size_t in_left = strlen(path_fs);
	size_t out_left = sizeof(path_utf8);

	size_t ret = g_iconv(conv, &in, &in_left, &out, &out_left);

	g_iconv_close(conv);

	if (ret == static_cast<size_t>(-1) || in_left > 0)
		return std::string();

	auto result_path = std::string(path_utf8, sizeof(path_utf8) - out_left);
	FixSeparators(result_path);
	return result_path;
#endif
}

#ifdef HAVE_GLIB

char *
PathFromUTF8(const char *path_utf8)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(path_utf8 != nullptr);
#endif

	if (fs_charset.empty())
		return g_strdup(path_utf8);

	return g_convert(path_utf8, -1,
			 fs_charset.c_str(), "utf-8",
			 nullptr, nullptr, nullptr);
}

#endif
