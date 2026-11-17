// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Reader.hxx"

/**
 * A #Reader implementation that concatenates two other #Reader
 * instances.
 */
class ConcatReader final : public Reader {
	Reader &a, &b;

public:
	[[nodiscard]]
	explicit ConcatReader(Reader &_a, Reader &_b) noexcept
		:a(_a), b(_b) {}

	std::size_t Read(std::span<std::byte> dest) override {
		std::size_t n = a.Read(dest);
		if (n == 0)
			n = b.Read(dest);
		return n;
	}
};
