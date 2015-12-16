/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "GzipOutputStream.hxx"
#include "lib/zlib/Domain.hxx"
#include "lib/zlib/Error.hxx"

GzipOutputStream::GzipOutputStream(OutputStream &_next) throw(ZlibError)
	:next(_next)
{
	z.next_in = nullptr;
	z.avail_in = 0;
	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	constexpr int windowBits = 15;
	constexpr int gzip_encoding = 16;

	int result = deflateInit2(&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
				  windowBits | gzip_encoding,
				  8, Z_DEFAULT_STRATEGY);
	if (result != Z_OK)
		throw ZlibError(result);
}

GzipOutputStream::~GzipOutputStream()
{
	deflateEnd(&z);
}

void
GzipOutputStream::Flush()
{
	/* no more input */
	z.next_in = nullptr;
	z.avail_in = 0;

	while (true) {
		Bytef output[4096];
		z.next_out = output;
		z.avail_out = sizeof(output);

		int result = deflate(&z, Z_FINISH);
		if (z.next_out > output)
			next.Write(output, z.next_out - output);

		if (result == Z_STREAM_END)
			break;
		else if (result != Z_OK)
			throw ZlibError(result);
	}
}

void
GzipOutputStream::Write(const void *_data, size_t size)
{
	/* zlib's API requires non-const input pointer */
	void *data = const_cast<void *>(_data);

	z.next_in = reinterpret_cast<Bytef *>(data);
	z.avail_in = size;

	while (z.avail_in > 0) {
		Bytef output[4096];
		z.next_out = output;
		z.avail_out = sizeof(output);

		int result = deflate(&z, Z_NO_FLUSH);
		if (result != Z_OK)
			throw ZlibError(result);

		if (z.next_out > output)
			next.Write(output, z.next_out - output);
	}
}
