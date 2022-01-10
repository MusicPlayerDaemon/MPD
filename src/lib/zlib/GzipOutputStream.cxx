/*
 * Copyright (C) 2014-2018 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

	constexpr int windowBits = 15;
	constexpr int gzip_encoding = 16;

	int result = deflateInit2(&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
				  windowBits | gzip_encoding,
				  8, Z_DEFAULT_STRATEGY);
	if (result != Z_OK)
		throw ZlibError(result);
}

GzipOutputStream::~GzipOutputStream()
{
	deflateEnd(&z);
}

void
GzipOutputStream::Flush()
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
			next.Write(output, z.next_out - output);

		if (result == Z_STREAM_END)
			break;
		else if (result != Z_OK)
			throw ZlibError(result);
	}
}

void
GzipOutputStream::Write(const void *_data, size_t size)
{
	/* zlib's API requires non-const input pointer */
	void *data = const_cast<void *>(_data);

	z.next_in = reinterpret_cast<Bytef *>(data);
	z.avail_in = size;

	while (z.avail_in > 0) {
		Bytef output[16384];
		z.next_out = output;
		z.avail_out = sizeof(output);

		int result = deflate(&z, Z_NO_FLUSH);
		if (result != Z_OK)
			throw ZlibError(result);

		if (z.next_out > output)
			next.Write(output, z.next_out - output);
	}
}
