/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "DecoderBuffer.hxx"
#include "DecoderAPI.hxx"

bool
DecoderBuffer::Fill()
{
	auto w = buffer.Write();
	if (w.empty())
		/* buffer is full */
		return false;

	size_t nbytes = decoder_read(client, is,
				     w.data, w.size);
	if (nbytes == 0)
		/* end of file, I/O error or decoder command
		   received */
		return false;

	buffer.Append(nbytes);
	return true;
}

ConstBuffer<void>
DecoderBuffer::Need(size_t min_size)
{
	while (true) {
		const auto r = Read();
		if (r.size >= min_size)
			return r;

		if (!Fill())
			return nullptr;
	}
}

bool
DecoderBuffer::Skip(size_t nbytes)
{
	const auto r = buffer.Read();
	if (r.size >= nbytes) {
		buffer.Consume(nbytes);
		return true;
	}

	buffer.Clear();
	nbytes -= r.size;

	return decoder_skip(client, is, nbytes);
}
