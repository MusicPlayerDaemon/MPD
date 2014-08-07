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

#ifndef MPD_BUFFERED_OUTPUT_STREAM_HXX
#define MPD_BUFFERED_OUTPUT_STREAM_HXX

#include "check.h"
#include "Compiler.h"
#include "util/DynamicFifoBuffer.hxx"
#include "util/Error.hxx"

#include <stddef.h>

class OutputStream;
class Error;

class BufferedOutputStream {
	OutputStream &os;

	DynamicFifoBuffer<char> buffer;

	Error last_error;

public:
	BufferedOutputStream(OutputStream &_os)
		:os(_os), buffer(32768) {}

	bool Write(const void *data, size_t size);
	bool Write(const char *p);

	gcc_printf(2,3)
	bool Format(const char *fmt, ...);

	gcc_pure
	bool Check() const {
		return !last_error.IsDefined();
	}

	bool Check(Error &error) const {
		if (last_error.IsDefined()) {
			error.Set(last_error);
			return false;
		} else
			return true;
	}

	bool Flush();

	bool Flush(Error &error);

private:
	bool AppendToBuffer(const void *data, size_t size);
};

#endif
