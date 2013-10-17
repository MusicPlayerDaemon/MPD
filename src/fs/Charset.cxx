/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>

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
	char *test = g_convert(" ", 1, charset, "UTF-8", NULL, NULL, NULL);
	if (test == NULL)
		return false;

	g_free(test);
	return true;
}

void
SetFSCharset(const char *charset)
{
	assert(charset != NULL);

	if (!IsSupportedCharset(charset))
		FormatFatalError("invalid filesystem charset: %s", charset);

	fs_charset = charset;

	FormatDebug(path_domain,
		    "SetFSCharset: fs charset is: %s", fs_charset.c_str());
}

const std::string &
GetFSCharset()
{
	return fs_charset;
}

std::string
PathToUTF8(const char *path_fs)
{
	assert(path_fs != nullptr);

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

	return std::string(path_utf8, sizeof(path_utf8) - out_left);
}

char *
PathFromUTF8(const char *path_utf8)
{
	assert(path_utf8 != nullptr);

	return g_convert(path_utf8, -1,
			 fs_charset.c_str(), "utf-8",
			 nullptr, nullptr, nullptr);
}
