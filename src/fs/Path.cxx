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
#include "fs/Path.hxx"
#include "ConfigGlobal.hxx"
#include "system/FatalError.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "gcc.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

#ifdef G_OS_WIN32
#include <windows.h> // for GetACP()
#include <stdio.h> // for sprintf()
#endif

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "path"

/**
 * Maximal number of bytes required to represent path name in UTF-8
 * (including nul-terminator).
 * This value is a rought estimate of upper bound.
 * It's based on path name limit in bytes (MPD_PATH_MAX)
 * and assumption that some weird encoding could represent some UTF-8 4 byte
 * sequences with single byte.
 */
#define MPD_PATH_MAX_UTF8 ((MPD_PATH_MAX - 1) * 4 + 1)

const Domain path_domain("path");

std::string fs_charset;

inline Path::Path(Donate, pointer _value)
	:value(_value) {
	g_free(_value);
}

/* no inlining, please */
Path::~Path() {}

Path
Path::Build(const_pointer a, const_pointer b)
{
	return Path(Donate(), g_build_filename(a, b, nullptr));
}

std::string Path::ToUTF8(const_pointer path_fs)
{
	if (path_fs == nullptr)
		return std::string();

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

Path Path::FromUTF8(const char *path_utf8)
{
	gchar *p;

	p = g_convert(path_utf8, -1,
		      fs_charset.c_str(), "utf-8",
		      NULL, NULL, NULL);

	return Path(Donate(), p);
}

Path
Path::FromUTF8(const char *path_utf8, Error &error)
{
	Path path = FromUTF8(path_utf8);
	if (path.IsNull())
		error.Format(path_domain,
			     "Failed to convert to file system charset: %s",
			     path_utf8);

	return path;
}

Path
Path::GetDirectoryName() const
{
	return Path(Donate(), g_path_get_dirname(value.c_str()));
}

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

static void
SetFSCharset(const char *charset)
{
	assert(charset != NULL);

	if (!IsSupportedCharset(charset))
		FormatFatalError("invalid filesystem charset: %s", charset);

	fs_charset = charset;

	g_debug("SetFSCharset: fs charset is: %s", fs_charset.c_str());
}

const std::string &Path::GetFSCharset()
{
	return fs_charset;
}

void Path::GlobalInit()
{
	const char *charset = NULL;

	charset = config_get_string(CONF_FS_CHARSET, NULL);
	if (charset == NULL) {
#ifndef G_OS_WIN32
		const gchar **encodings;
		g_get_filename_charsets(&encodings);

		if (encodings[0] != NULL && *encodings[0] != '\0')
			charset = encodings[0];
#else /* G_OS_WIN32 */
		/* Glib claims that file system encoding is always utf-8
		 * on native Win32 (i.e. not Cygwin).
		 * However this is true only if <gstdio.h> helpers are used.
		 * MPD uses regular <stdio.h> functions.
		 * Those functions use encoding determined by GetACP(). */
		static char win_charset[13];
		sprintf(win_charset, "cp%u", GetACP());
		charset = win_charset;
#endif
	}

	if (charset) {
		SetFSCharset(charset);
	} else {
		g_message("setting filesystem charset to ISO-8859-1");
		SetFSCharset("ISO-8859-1");
	}
}
