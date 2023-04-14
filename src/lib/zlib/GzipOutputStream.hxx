// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef GZIP_OUTPUT_STREAM_HXX
#define GZIP_OUTPUT_STREAM_HXX

#include "io/OutputStream.hxx"

#include <zlib.h>

/**
 * A filter that compresses data written to it using zlib, forwarding
 * compressed data in the "gzip" format.
 *
 * Don't forget to call Flush() before destructing this object.
 */
class GzipOutputStream final : public OutputStream {
	OutputStream &next;

	z_stream z;

public:
	/**
	 * Construct the filter.
	 *
	 * Throws #ZlibError on error.
	 */
	explicit GzipOutputStream(OutputStream &_next);
	~GzipOutputStream() noexcept;

	/**
	 * Throws on error.
	 */
	void SyncFlush();

	/**
	 * Finish the file and write all data remaining in zlib's
	 * output buffer.
	 *
	 * Throws on error.
	 */
	void Finish();

	/* virtual methods from class OutputStream */
	void Write(std::span<const std::byte> src) override;
};

#endif
