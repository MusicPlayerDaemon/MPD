/*
 * Copyright 2014-2019 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef BUFFERED_OUTPUT_STREAM_HXX
#define BUFFERED_OUTPUT_STREAM_HXX

#include "util/Compiler.h"
#include "util/DynamicFifoBuffer.hxx"

#include <stddef.h>

#ifdef _UNICODE
#include <wchar.h>
#endif

class OutputStream;

/**
 * An #OutputStream wrapper that buffers its output to reduce the
 * number of OutputStream::Write() calls.
 *
 * All wchar_t based strings are converted to UTF-8.
 */
class BufferedOutputStream {
	OutputStream &os;

	DynamicFifoBuffer<char> buffer;

public:
	explicit BufferedOutputStream(OutputStream &_os) noexcept
		:os(_os), buffer(32768) {}

	void Write(const void *data, size_t size);

	void Write(const char &ch) {
		Write(&ch, sizeof(ch));
	}

	void Write(const char *p);

	gcc_printf(2,3)
	void Format(const char *fmt, ...);

#ifdef _UNICODE
	void Write(const wchar_t &ch) {
		WriteWideToUTF8(&ch, 1);
	}

	void Write(const wchar_t *p);
#endif

	/**
	 * Write buffer contents to the #OutputStream.
	 */
	void Flush();

private:
	bool AppendToBuffer(const void *data, size_t size) noexcept;

#ifdef _UNICODE
	void WriteWideToUTF8(const wchar_t *p, size_t length);
#endif
};

template<typename F>
void
WithBufferedOutputStream(OutputStream &os, F &&f)
{
	BufferedOutputStream bos(os);
	f(bos);
	bos.Flush();
}

#endif
