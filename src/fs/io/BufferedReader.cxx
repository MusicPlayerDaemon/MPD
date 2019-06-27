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

#include "BufferedReader.hxx"
#include "Reader.hxx"
#include "util/TextFile.hxx"

#include <stdexcept>

#include <stdint.h>
#include <string.h>

bool
BufferedReader::Fill(bool need_more)
{
	if (eof)
		return !need_more;

	auto w = buffer.Write();
	if (w.empty()) {
		if (buffer.GetCapacity() >= MAX_SIZE)
			return !need_more;

		buffer.Grow(buffer.GetCapacity() * 2);
		w = buffer.Write();
		assert(!w.empty());
	}

	size_t nbytes = reader.Read(w.data, w.size);
	if (nbytes == 0) {
		eof = true;
		return !need_more;
	}

	buffer.Append(nbytes);
	return true;
}

void *
BufferedReader::ReadFull(size_t size)
{
	while (true) {
		auto r = Read();
		if (r.size >= size)
			return r.data;

		if (!Fill(true))
			throw std::runtime_error("Premature end of file");
	}
}

size_t
BufferedReader::ReadFromBuffer(WritableBuffer<void> dest) noexcept
{
	auto src = Read();
	size_t nbytes = std::min(src.size, dest.size);
	memcpy(dest.data, src.data, nbytes);
	Consume(nbytes);
	return nbytes;
}

void
BufferedReader::ReadFull(WritableBuffer<void> _dest)
{
	auto dest = WritableBuffer<uint8_t>::FromVoid(_dest);
	assert(dest.size == _dest.size);

	while (true) {
		size_t nbytes = ReadFromBuffer(dest.ToVoid());
		dest.skip_front(nbytes);
		if (dest.size == 0)
			break;

		if (!Fill(true))
			throw std::runtime_error("Premature end of file");
	}
}

char *
BufferedReader::ReadLine()
{
	do {
		char *line = ReadBufferedLine(buffer);
		if (line != nullptr) {
			++line_number;
			return line;
		}
	} while (Fill(true));

	if (!eof || buffer.empty())
		return nullptr;

	auto w = buffer.Write();
	if (w.empty()) {
		buffer.Grow(buffer.GetCapacity() + 1);
		w = buffer.Write();
		assert(!w.empty());
	}

	/* terminate the last line */
	w[0] = 0;

	char *line = buffer.Read().data;
	buffer.Clear();
	++line_number;
	return line;
}
