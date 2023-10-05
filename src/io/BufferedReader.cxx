// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "BufferedReader.hxx"
#include "Reader.hxx"
#include "util/TextFile.hxx"

#include <algorithm> // for std::copy_n()
#include <cassert>
#include <cstdint>
#include <stdexcept>

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

	std::size_t nbytes = reader.Read(w);
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
		if (r.size() >= size)
			return r.data();

		if (!Fill(true))
			throw std::runtime_error("Premature end of file");
	}
}

std::size_t
BufferedReader::ReadFromBuffer(std::span<std::byte> dest) noexcept
{
	const auto src = Read();
	std::size_t nbytes = std::min(src.size(), dest.size());
	std::copy_n(src.data(), nbytes, dest.data());
	Consume(nbytes);
	return nbytes;
}

void
BufferedReader::ReadFull(std::span<std::byte> dest)
{
	while (true) {
		std::size_t nbytes = ReadFromBuffer(dest);
		dest = dest.subspan(nbytes);
		if (dest.empty())
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
	w[0] = {};

	char *line = reinterpret_cast<char *>(buffer.Read().data());
	buffer.Clear();
	++line_number;
	return line;
}
