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
#include "DecoderBuffer.hxx"
#include "DecoderAPI.hxx"
#include "util/ConstBuffer.hxx"

#include <assert.h>

const InputStream &
decoder_buffer_get_stream(const DecoderBuffer *buffer)
{
	return buffer->is;
}

void
decoder_buffer_clear(DecoderBuffer *buffer)
{
	buffer->buffer.Clear();
}

bool
decoder_buffer_fill(DecoderBuffer *buffer)
{
	auto w = buffer->buffer.Write();
	if (w.IsEmpty())
		/* buffer is full */
		return false;

	size_t nbytes = decoder_read(buffer->decoder, buffer->is,
				     w.data, w.size);
	if (nbytes == 0)
		/* end of file, I/O error or decoder command
		   received */
		return false;

	buffer->buffer.Append(nbytes);
	return true;
}

size_t
decoder_buffer_available(const DecoderBuffer *buffer)
{
	return buffer->buffer.GetAvailable();
}

ConstBuffer<void>
decoder_buffer_read(const DecoderBuffer *buffer)
{
	auto r = buffer->buffer.Read();
	return { r.data, r.size };
}

ConstBuffer<void>
decoder_buffer_need(DecoderBuffer *buffer, size_t min_size)
{
	while (true) {
		const auto r = decoder_buffer_read(buffer);
		if (r.size >= min_size)
			return r;

		if (!decoder_buffer_fill(buffer))
			return nullptr;
	}
}

void
decoder_buffer_consume(DecoderBuffer *buffer, size_t nbytes)
{
	buffer->buffer.Consume(nbytes);
}

bool
decoder_buffer_skip(DecoderBuffer *buffer, size_t nbytes)
{
	const auto r = buffer->buffer.Read();
	if (r.size >= nbytes) {
		buffer->buffer.Consume(nbytes);
		return true;
	}

	buffer->buffer.Clear();
	nbytes -= r.size;

	return decoder_skip(buffer->decoder, buffer->is, nbytes);
}
