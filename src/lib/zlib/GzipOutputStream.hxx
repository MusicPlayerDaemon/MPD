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
	 */
	explicit GzipOutputStream(OutputStream &_next);
	~GzipOutputStream();

	/**
	 * Finish the file and write all data remaining in zlib's
	 * output buffer.
	 */
	void Flush();

	/* virtual methods from class OutputStream */
	void Write(const void *data, size_t size) override;
};

#endif
