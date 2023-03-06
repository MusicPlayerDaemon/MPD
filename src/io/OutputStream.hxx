// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef OUTPUT_STREAM_HXX
#define OUTPUT_STREAM_HXX

#include <cstddef>

class OutputStream {
public:
	OutputStream() = default;
	OutputStream(const OutputStream &) = delete;

	/**
	 * Throws std::exception on error.
	 */
	virtual void Write(const void *data, std::size_t size) = 0;
};

#endif
