// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef>
#include <span>

class OutputStream {
public:
	OutputStream() = default;
	OutputStream(const OutputStream &) = delete;

	/**
	 * Throws std::exception on error.
	 */
	virtual void Write(std::span<const std::byte> src) = 0;
};
