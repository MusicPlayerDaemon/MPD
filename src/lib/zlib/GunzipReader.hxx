// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "io/Reader.hxx"
#include "util/StaticFifoBuffer.hxx"

#include <zlib.h>

/**
 * A filter that decompresses data using zlib.
 */
class GunzipReader final : public Reader {
	Reader &next;

	bool eof = false;

	z_stream z;

	StaticFifoBuffer<std::byte, 65536> buffer;

public:
	/**
	 * Construct the filter.
	 *
	 * Throws on error.
	 */
	explicit GunzipReader(Reader &_next);

	~GunzipReader() noexcept {
		inflateEnd(&z);
	}

	/* virtual methods from class Reader */
	std::size_t Read(std::span<std::byte> dest) override;

private:
	bool FillBuffer();
};
