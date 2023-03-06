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
		size_t nbytes = next.Read(buffer + buffer_size,
					  size - buffer_size);
		if (nbytes == 0)
			return nullptr;

		buffer_size += nbytes;
	} while (buffer_size < size);

	return buffer;
}

size_t
PeekReader::Read(void *data, size_t size)
{
	size_t buffer_remaining = buffer_size - buffer_position;
	if (buffer_remaining > 0) {
		size_t nbytes = std::min(buffer_remaining, size);
		memcpy(data, buffer + buffer_position, nbytes);
		buffer_position += nbytes;
		return nbytes;
	}

	return next.Read(data, size);
}
