// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PeekReader.hxx"

#include <algorithm>
#include <cassert>

#include <string.h>

const void *
PeekReader::Peek(size_t size)
{
	assert(size > 0);
	assert(size < sizeof(buffer));
	assert(buffer_size == 0);
	assert(buffer_position == 0);

	do {
		size_t nbytes = next.Read(std::span{buffer}.first(size).subspan(buffer_size));
		if (nbytes == 0)
			return nullptr;

		buffer_size += nbytes;
	} while (buffer_size < size);

	return buffer;
}

size_t
PeekReader::Read(std::span<std::byte> dest)
{
	auto src = std::span{buffer}.first(buffer_size).subspan(buffer_position);
	if (!src.empty()) {
		if (dest.size() < src.size())
			src = src.first(dest.size());

		std::copy(src.begin(), src.end(), dest.begin());
		buffer_position += src.size();
		return src.size();
	}

	return next.Read(dest);
}
