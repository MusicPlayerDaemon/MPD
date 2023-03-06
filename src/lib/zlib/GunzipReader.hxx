// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef GUNZIP_READER_HXX
#define GUNZIP_READER_HXX

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

	StaticFifoBuffer<Bytef, 65536> buffer;

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
	std::size_t Read(void *data, std::size_t size) override;

private:
	bool FillBuffer();
};

#endif
