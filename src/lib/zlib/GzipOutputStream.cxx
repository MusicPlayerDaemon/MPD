// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "GzipOutputStream.hxx"
#include "Error.hxx"

GzipOutputStream::GzipOutputStream(OutputStream &_next)
	:next(_next)
{
	z.next_in = nullptr;
	z.avail_in = 0;
	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	constexpr int windowBits = MAX_WBITS;
	constexpr int gzip_encoding = 16;

	int result = deflateInit2(&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
				  windowBits | gzip_encoding,
				  8, Z_DEFAULT_STRATEGY);
	if (result != Z_OK)
		throw ZlibError(result);
}

GzipOutputStream::~GzipOutputStream() noexcept
{
	deflateEnd(&z);
}

void
GzipOutputStream::SyncFlush()
{
	/* no more input */
	z.next_in = nullptr;
	z.avail_in = 0;

	do {
		Bytef output[16384];
		z.next_out = output;
		z.avail_out = sizeof(output);

		int result = deflate(&z, Z_SYNC_FLUSH);
		if (result != Z_OK)
			throw ZlibError(result);

		if (z.next_out == output)
			break;

		next.Write(std::as_bytes(std::span{output}.first(z.next_out - output)));
	} while (z.avail_out == 0);
}

void
GzipOutputStream::Finish()
{
	/* no more input */
	z.next_in = nullptr;
	z.avail_in = 0;

	while (true) {
		Bytef output[16384];
		z.next_out = output;
		z.avail_out = sizeof(output);

		int result = deflate(&z, Z_FINISH);
		if (z.next_out > output)
			next.Write(std::as_bytes(std::span{output}.first(z.next_out - output)));

		if (result == Z_STREAM_END)
			break;
		else if (result != Z_OK)
			throw ZlibError(result);
	}
}

void
GzipOutputStream::Write(std::span<const std::byte> src)
{
	/* zlib's API requires non-const input pointer */
	void *data = const_cast<std::byte *>(src.data());

	z.next_in = reinterpret_cast<Bytef *>(data);
	z.avail_in = src.size();

	while (z.avail_in > 0) {
		Bytef output[65536];
		z.next_out = output;
		z.avail_out = sizeof(output);

		int result = deflate(&z, Z_NO_FLUSH);
		if (result != Z_OK)
			throw ZlibError(result);

		if (z.next_out > output)
			next.Write(std::as_bytes(std::span{output}.first(z.next_out - output)));
	}
}
