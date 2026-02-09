// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "util/RoundPowerOfTwo.hxx"

#include <cstddef>

#include <unistd.h> // for sysconf()

/**
 * Round up the parameter, make it page-aligned.
 */
[[gnu::const]]
static std::size_t
AlignToPageSize(std::size_t size) noexcept
{
	static const long page_size = sysconf(_SC_PAGESIZE);
	if (page_size <= 0)
		return size;

	return RoundUpToPowerOfTwo(size, static_cast<std::size_t>(page_size));
}
