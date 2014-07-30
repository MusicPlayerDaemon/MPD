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
#include "BufferedOutputStream.hxx"
#include "OutputStream.hxx"

#include <stdarg.h>
#include <string.h>
#include <stdio.h>

bool
BufferedOutputStream::AppendToBuffer(const void *data, size_t size)
{
	auto r = buffer.Write();
	if (r.size < size)
		return false;

	memcpy(r.data, data, size);
	buffer.Append(size);
	return true;
}

bool
BufferedOutputStream::Write(const void *data, size_t size)
{
	if (gcc_unlikely(last_error.IsDefined()))
		return false;

	if (AppendToBuffer(data, size))
		return true;

	if (!Flush())
		return false;

	if (AppendToBuffer(data, size))
		return true;

	return os.Write(data, size, last_error);
}

bool
BufferedOutputStream::Write(const char *p)
{
	return Write(p, strlen(p));
}

bool
BufferedOutputStream::Format(const char *fmt, ...)
{
	if (gcc_unlikely(last_error.IsDefined()))
		return false;

	auto r = buffer.Write();
	if (r.IsEmpty()) {
		if (!Flush())
			return false;

		r = buffer.Write();
	}

	/* format into the buffer */
	va_list ap;
	va_start(ap, fmt);
	size_t size = vsnprintf(r.data, r.size, fmt, ap);
	va_end(ap);

	if (gcc_unlikely(size >= r.size)) {
		/* buffer was not large enough; flush it and try
		   again */

		if (!Flush())
			return false;

		r = buffer.Write();

		if (gcc_unlikely(size >= r.size)) {
			/* still not enough space: grow the buffer and
			   try again */
			r.size = size + 1;
			r.data = buffer.Write(r.size);
		}

		/* format into the new buffer */
		va_start(ap, fmt);
		size = vsnprintf(r.data, r.size, fmt, ap);
		va_end(ap);

		/* this time, it must fit */
		assert(size < r.size);
	}

	buffer.Append(size);
	return true;
}

bool
BufferedOutputStream::Flush()
{
	if (!Check())
		return false;

	auto r = buffer.Read();
	if (r.IsEmpty())
		return true;

	bool success = os.Write(r.data, r.size, last_error);
	if (gcc_likely(success))
		buffer.Consume(r.size);
	return success;
}

bool
BufferedOutputStream::Flush(Error &error)
{
	if (!Check(error))
		return false;

	auto r = buffer.Read();
	if (r.IsEmpty())
		return true;

	bool success = os.Write(r.data, r.size, error);
	if (gcc_likely(success))
		buffer.Consume(r.size);
	return success;
}
