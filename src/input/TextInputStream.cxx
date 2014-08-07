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
#include "TextInputStream.hxx"
#include "InputStream.hxx"
#include "util/StringUtil.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <assert.h>
#include <string.h>

char *
TextInputStream::ReadBufferedLine()
{
	auto r = buffer.Read();
	char *newline = reinterpret_cast<char*>(memchr(r.data, '\n', r.size));
	if (newline == nullptr)
		return nullptr;

	buffer.Consume(newline + 1 - r.data);

	char *end = StripRight(r.data, newline);
	*end = 0;
	return r.data;
}

char *
TextInputStream::ReadLine()
{
	char *line = ReadBufferedLine();
	if (line != nullptr)
		return line;

	while (true) {
		auto dest = buffer.Write();
		if (dest.size < 2) {
			/* end of file (or line too long): terminate
			   the current line */

			assert(!dest.IsEmpty());
			dest[0] = 0;
			line = buffer.Read().data;
			buffer.Clear();
			return line;
		}

		/* reserve one byte for the null terminator if the
		   last line is not terminated by a newline
		   character */
		--dest.size;

		Error error;
		size_t nbytes = is.LockRead(dest.data, dest.size, error);
		if (nbytes > 0)
			buffer.Append(nbytes);
		else if (error.IsDefined()) {
			LogError(error);
			return nullptr;
		}

		line = ReadBufferedLine();
		if (line != nullptr)
			return line;

		if (nbytes == 0)
			return nullptr;
	}
}
