/*
 * Copyright 2014-2019 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "BufferedReader.hxx"
#include "Reader.hxx"
#include "util/TextFile.hxx"

#include <cassert>
#include <cstdint>
#include <stdexcept>

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

	std::size_t nbytes = reader.Read(w.data, w.size);
	if (nbytes == 0) {
		eof = true;
		return !need_more;
	}

	buffer.Append(nbytes);
	return true;
}

void *
BufferedReader::ReadFull(std::size_t size)
{
	while (true) {
		auto r = Read();
		if (r.size >= size)
			return r.data;

		if (!Fill(true))
			throw std::runtime_error("Premature end of file");
	}
}

std::size_t
BufferedReader::ReadFromBuffer(WritableBuffer<void> dest) noexcept
{
	auto src = Read();
	std::size_t nbytes = std::min(src.size, dest.size);
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
		std::size_t nbytes = ReadFromBuffer(dest.ToVoid());
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
