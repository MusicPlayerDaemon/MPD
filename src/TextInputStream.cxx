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
#include "util/CharUtil.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <assert.h>
#include <string.h>

bool TextInputStream::ReadLine(std::string &line)
{
	const char *src, *p;

	do {
		size_t nbytes;
		auto dest = buffer.Write();
		if (dest.size >= 2) {
			/* reserve one byte for the null terminator if
			   the last line is not terminated by a
			   newline character */
			--dest.size;

			Error error;
			nbytes = is.LockRead(dest.data, dest.size, error);
			if (nbytes > 0)
				buffer.Append(nbytes);
			else if (error.IsDefined()) {
				LogError(error);
				return false;
			}
		} else
			nbytes = 0;

		auto src_p = buffer.Read();
		if (src_p.IsEmpty())
			return false;

		src = src_p.data;

		p = reinterpret_cast<const char*>(memchr(src, '\n', src_p.size));
		if (p == nullptr && nbytes == 0) {
			/* end of file (or line too long): terminate
			   the current line */
			dest = buffer.Write();
			assert(!dest.IsEmpty());
			dest.data[0] = '\n';
			buffer.Append(1);
		}
	} while (p == nullptr);

	size_t length = p - src + 1;
	while (p > src && IsWhitespaceOrNull(p[-1]))
		--p;

	line = std::string(src, p - src);
	buffer.Consume(length);
	return true;
}
