// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef OUTPUT_STREAM_HXX
#define OUTPUT_STREAM_HXX

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

#endif
