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
#include "TextInputStream.hxx"
#include "InputLegacy.hxx"
#include "util/fifo_buffer.h"
#include "util/Error.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>

TextInputStream::TextInputStream(struct input_stream *_is)
	: is(_is),
	  buffer(fifo_buffer_new(4096))
{
}

TextInputStream::~TextInputStream()
{
	fifo_buffer_free(buffer);
}

bool TextInputStream::ReadLine(std::string &line)
{
	void *dest;
	const char *src, *p;
	size_t length, nbytes;

	do {
		dest = fifo_buffer_write(buffer, &length);
		if (dest != nullptr && length >= 2) {
			/* reserve one byte for the null terminator if
			   the last line is not terminated by a
			   newline character */
			--length;

			Error error;
			nbytes = input_stream_lock_read(is, dest, length,
							error);
			if (nbytes > 0)
				fifo_buffer_append(buffer, nbytes);
			else if (error.IsDefined()) {
				g_warning("%s", error.GetMessage());
				return false;
			}
		} else
			nbytes = 0;

		auto src_p = fifo_buffer_read(buffer, &length);
		src = reinterpret_cast<const char *>(src_p);

		if (src == nullptr)
			return false;

		p = reinterpret_cast<const char*>(memchr(src, '\n', length));
		if (p == nullptr && nbytes == 0) {
			/* end of file (or line too long): terminate
			   the current line */
			dest = fifo_buffer_write(buffer, &nbytes);
			assert(dest != nullptr);
			*(char *)dest = '\n';
			fifo_buffer_append(buffer, 1);
		}
	} while (p == nullptr);

	length = p - src + 1;
	while (p > src && g_ascii_isspace(p[-1]))
		--p;

	line = std::string(src, p - src);
	fifo_buffer_consume(buffer, length);
	return true;
}
