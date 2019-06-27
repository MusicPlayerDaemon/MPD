/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "GunzipReader.hxx"
#include "lib/zlib/Error.hxx"

GunzipReader::GunzipReader(Reader &_next)
	:next(_next)
{
	z.next_in = nullptr;
	z.avail_in = 0;
	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	int result = inflateInit2(&z, 16 + MAX_WBITS);
	if (result != Z_OK)
		throw ZlibError(result);
}

inline bool
GunzipReader::FillBuffer()
{
	auto w = buffer.Write();
	assert(!w.empty());

	size_t nbytes = next.Read(w.data, w.size);
	if (nbytes == 0)
		return false;

	buffer.Append(nbytes);
	return true;
}

size_t
GunzipReader::Read(void *data, size_t size)
{
	if (eof)
		return 0;

	z.next_out = (Bytef *)data;
	z.avail_out = size;

	while (true) {
		int flush = Z_NO_FLUSH;

		auto r = buffer.Read();
		if (r.empty()) {
			if (FillBuffer())
				r = buffer.Read();
			else
				flush = Z_FINISH;
		}

		z.next_in = r.data;
		z.avail_in = r.size;

		int result = inflate(&z, flush);
		if (result == Z_STREAM_END) {
			eof = true;
			return size - z.avail_out;
		} else if (result != Z_OK)
			throw ZlibError(result);

		buffer.Consume(r.size - z.avail_in);

		if (z.avail_out < size)
			return size - z.avail_out;
	}
}
