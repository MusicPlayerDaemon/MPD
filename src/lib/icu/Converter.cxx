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
#include "Converter.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <string.h>

#ifdef HAVE_GLIB
static constexpr Domain g_iconv_domain("g_iconv");
#endif

#ifdef HAVE_ICU_CONVERTER

IcuConverter *
IcuConverter::Create(const char *charset, Error &error)
{
	GIConv to = g_iconv_open("utf-8", charset);
	GIConv from = g_iconv_open(charset, "utf-8");
	if (to == (GIConv)-1 || from == (GIConv)-1) {
		if (to != (GIConv)-1)
			g_iconv_close(to);
		if (from != (GIConv)-1)
			g_iconv_close(from);
		error.Format(g_iconv_domain,
			     "Failed to initialize charset '%s'", charset);
		return nullptr;
	}

	return new IcuConverter(to, from);
}

static std::string
DoConvert(GIConv conv, const char *src)
{
	// TODO: dynamic buffer?
	char buffer[4096];
	char *in = const_cast<char *>(src);
	char *out = buffer;
	size_t in_left = strlen(src);
	size_t out_left = sizeof(buffer);

	size_t n = g_iconv(conv, &in, &in_left, &out, &out_left);

	if (n == static_cast<size_t>(-1) || in_left > 0)
		return std::string();

	return std::string(buffer, sizeof(buffer) - out_left);
}

std::string
IcuConverter::ToUTF8(const char *s) const
{
	return DoConvert(to_utf8, s);
}

std::string
IcuConverter::FromUTF8(const char *s) const
{
	return DoConvert(from_utf8, s);
}

#endif
