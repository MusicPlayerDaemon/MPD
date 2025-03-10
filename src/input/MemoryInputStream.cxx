// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "MemoryInputStream.hxx"

#include <algorithm> // for std::copy()

void
MemoryInputStream::Seek(std::unique_lock<Mutex> &,
			offset_type new_offset)
{
	if (std::cmp_greater(new_offset, src.size()))
		throw std::runtime_error{"Bad offset"};

	offset = new_offset;
}

size_t
MemoryInputStream::Read(std::unique_lock<Mutex> &, std::span<std::byte> dest)
{
	const std::size_t _offset = static_cast<std::size_t>(offset);
	std::size_t remaining = src.size() - _offset;
	std::size_t nbytes = std::min(dest.size(), remaining);

	const auto s = src.subspan(_offset, nbytes);
	std::copy(s.begin(), s.end(), dest.begin());
	return nbytes;
}
