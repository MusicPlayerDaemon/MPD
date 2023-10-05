// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Reader.hxx"

#include <stdexcept>

void
Reader::ReadFull(std::span<std::byte> dest)
{
	const auto nbytes = Read(dest);
	if (nbytes < dest.size())
		throw std::runtime_error{"Unexpected end of file"};
}
